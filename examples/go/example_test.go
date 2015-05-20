/*
Licensed to the Apache Software Foundation (ASF) under one
or more contributor license agreements.  See the NOTICE file
distributed with this work for additional information
regarding copyright ownership.  The ASF licenses this file
to you under the Apache License, Version 2.0 (the
"License"); you may not use this file except in compliance
with the License.  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing,
software distributed under the License is distributed on an
"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
KIND, either express or implied.  See the License for the
specific language governing permissions and limitations
under the License.
*/

// Tests to verify that example code behaves as expected.
// Run in this directory with `go test example_test.go`
//
package main

import (
	"bufio"
	"bytes"
	"flag"
	"fmt"
	"io"
	"io/ioutil"
	"math/rand"
	"net"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"reflect"
	"testing"
	"time"
)

func panicIf(err error) {
	if err != nil {
		panic(err)
	}
}

// A demo broker process
type broker struct {
	cmd    *exec.Cmd
	addr   string
	runerr chan error
	err    error
}

// Try to connect to the broker to verify it is ready, give up after a timeout
func (b *broker) check() error {
	dialer := net.Dialer{Deadline: time.Now().Add(time.Second * 10)}
	for {
		c, err := dialer.Dial("tcp", b.addr)
		if err == nil { // Success
			c.Close()
			return nil
		}
		select {
		case runerr := <-b.runerr: // Broker exited.
			return runerr
		default:
		}
		if neterr, ok := err.(net.Error); ok && neterr.Timeout() { // Running but timed out
			b.stop()
			return fmt.Errorf("timed out waiting for broker")
		}
		time.Sleep(time.Second / 10)
	}
}

// Start the demo broker, wait till it is listening on *addr. No-op if already started.
func (b *broker) start() error {
	build("event_broker.go")
	if b.cmd == nil { // Not already started
		// FIXME aconway 2015-04-30: better way to pick/configure a broker port.
		b.addr = fmt.Sprintf("127.0.0.1:%d", rand.Intn(10000)+10000)
		b.cmd = exec.Command(exepath("event_broker"), "-addr", b.addr, "-verbose", "0")
		b.runerr = make(chan error)
		// Change the -verbose setting above to see broker output on stdout/stderr.
		b.cmd.Stderr, b.cmd.Stdout = os.Stderr, os.Stdout
		go func() {
			b.runerr <- b.cmd.Run()
		}()
		b.err = b.check()
	}
	return b.err
}

func (b *broker) stop() {
	if b != nil && b.cmd != nil {
		b.cmd.Process.Kill()
		b.cmd.Wait()
	}
}

func checkEqual(want interface{}, got interface{}) error {
	if reflect.DeepEqual(want, got) {
		return nil
	}
	return fmt.Errorf("%#v != %#v", want, got)
}

// runCommand returns an exec.Cmd to run an example.
func exampleCommand(prog string, arg ...string) *exec.Cmd {
	build(prog + ".go")
	cmd := exec.Command(exepath(prog), arg...)
	cmd.Stderr = os.Stderr
	return cmd
}

// Run an example Go program, return the combined output as a string.
func runExample(prog string, arg ...string) (string, error) {
	cmd := exampleCommand(prog, arg...)
	out, err := cmd.Output()
	return string(out), err
}

func prefix(prefix string, err error) error {
	if err != nil {
		return fmt.Errorf("%s: %s", prefix, err)
	}
	return nil
}

func runExampleWant(want string, prog string, args ...string) error {
	out, err := runExample(prog, args...)
	if err != nil {
		return fmt.Errorf("%s failed: %s: %s", prog, err, out)
	}
	return prefix(prog, checkEqual(want, out))
}

func exampleArgs(args ...string) []string {
	return append(args, testBroker.addr+"/foo", testBroker.addr+"/bar", testBroker.addr+"/baz")
}

// Send then receive
func TestExampleSendReceive(t *testing.T) {
	if testing.Short() {
		t.Skip("Skip demo tests in short mode")
	}
	testBroker.start()
	err := runExampleWant(
		"send: Received all 15 acknowledgements\n",
		"send",
		exampleArgs("-count", "5", "-verbose", "1")...)
	if err != nil {
		t.Fatal(err)
	}
	err = runExampleWant(
		"receive: Listening\nreceive: Received 15 messages\n",
		"receive",
		exampleArgs("-verbose", "1", "-count", "15")...)
	if err != nil {
		t.Fatal(err)
	}
}

var ready error

func init() { ready = fmt.Errorf("Ready") }

// Run receive in a goroutine.
// Send ready on errchan when it is listening.
// Send final error when it is done.
// Returns the Cmd, caller must Wait()
func goReceiveWant(errchan chan<- error, want string, arg ...string) *exec.Cmd {
	cmd := exampleCommand("receive", arg...)
	go func() {
		pipe, err := cmd.StdoutPipe()
		if err != nil {
			errchan <- err
			return
		}
		out := bufio.NewReader(pipe)
		cmd.Start()
		line, err := out.ReadString('\n')
		if err != nil && err != io.EOF {
			errchan <- err
			return
		}
		listening := "receive: Listening\n"
		if line != listening {
			errchan <- checkEqual(listening, line)
			return
		}
		errchan <- ready
		buf := bytes.Buffer{}
		io.Copy(&buf, out) // Collect the rest of the output
		errchan <- checkEqual(want, buf.String())
		close(errchan)
	}()
	return cmd
}

// Start receiver first, wait till it is running, then send.
func TestExampleReceiveSend(t *testing.T) {
	if testing.Short() {
		t.Skip("Skip demo tests in short mode")
	}
	testBroker.start()
	recvErr := make(chan error)
	recvCmd := goReceiveWant(recvErr,
		"receive: Received 15 messages\n",
		exampleArgs("-count", "15", "-verbose", "1")...)
	defer func() {
		recvCmd.Process.Kill()
		recvCmd.Wait()
	}()
	if err := <-recvErr; err != ready { // Wait for receiver ready
		t.Fatal(err)
	}
	err := runExampleWant(
		"send: Received all 15 acknowledgements\n",
		"send",
		exampleArgs("-count", "5", "-verbose", "1")...)
	if err != nil {
		t.Fatal(err)
	}
	if err := <-recvErr; err != nil {
		t.Fatal(err)
	}
}

func exepath(relative string) string {
	if binDir == "" {
		panic("bindir not set, cannot run example binaries")
	}
	return path.Join(binDir, relative)
}

var testBroker *broker
var binDir, exampleDir string
var built map[string]bool

func init() {
	built = make(map[string]bool)
}

func build(prog string) {
	if !built[prog] {
		args := []string{"build"}
		if *rpath != "" {
			args = append(args, "-ldflags", "-r "+*rpath)
		}
		args = append(args, path.Join(exampleDir, prog))
		build := exec.Command("go", args...)
		build.Dir = binDir
		out, err := build.CombinedOutput()
		if err != nil {
			panic(fmt.Errorf("%v: %s", err, out))
		}
		built[prog] = true
	}
}

var rpath = flag.String("rpath", "", "Runtime path for test executables")

func TestMain(m *testing.M) {
	rand.Seed(time.Now().UTC().UnixNano())
	var err error
	exampleDir, err = filepath.Abs(".")
	panicIf(err)
	binDir, err = ioutil.TempDir("", "example_test.go")
	panicIf(err)
	defer os.Remove(binDir) // Clean up binaries
	testBroker = &broker{}  // Broker is started on-demand by tests.
	testBroker.stop()
	status := m.Run()
	testBroker.stop()
	os.Exit(status)
}
