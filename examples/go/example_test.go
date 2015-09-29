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
	"math/rand"
	"net"
	"os"
	"os/exec"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
	"time"
)

func fatalIf(t *testing.T, err error) {
	if err != nil {
		t.Fatalf("%s", err)
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
func (b *broker) start(t *testing.T) error {
	if b.cmd == nil { // Not already started
		b.addr = fmt.Sprintf("127.0.0.1:%d", rand.Intn(10000)+10000)
		b.cmd = exampleCommand(t, *brokerName, "-addr", b.addr)
		b.runerr = make(chan error)
		b.cmd.Stderr, b.cmd.Stdout = os.Stderr, os.Stdout
		b.err = b.cmd.Start()
		if b.err == nil {
			go func() { b.runerr <- b.cmd.Wait() }()
		} else {
			b.runerr <- b.err
		}
		b.err = b.check()
	}
	return b.err
}

func (b *broker) stop() {
	if b != nil && b.cmd != nil {
		b.cmd.Process.Kill()
		<-b.runerr
	}
}

func checkEqual(want interface{}, got interface{}) error {
	if reflect.DeepEqual(want, got) {
		return nil
	}
	return fmt.Errorf("%#v != %#v", want, got)
}

// 'go build' uses the installed copy of the proton Go libraries, which may be out of date.
func checkStaleLibs(t *testing.T) {
	var stale []string
	pp := "qpid.apache.org"
	for _, p := range []string{pp + "/proton", pp + "/amqp", pp + "/electron"} {
		out, err := exec.Command("go", "list", "-f", "{{.Stale}}", p).CombinedOutput()
		if err != nil {
			t.Fatalf("failed to execute 'go list': %v\n%v", err, string(out))
		}
		if string(out) != "false\n" {
			stale = append(stale, p)
		}
	}
	if len(stale) > 0 {
		t.Fatalf("Stale libraries, run 'go install %s'", strings.Trim(fmt.Sprint(stale), "[]"))
	}
}

// exampleCommand returns an exec.Cmd to run an example.
func exampleCommand(t *testing.T, prog string, arg ...string) (cmd *exec.Cmd) {
	checkStaleLibs(t)
	args := []string{}
	if *debug {
		args = append(args, "-debug=true")
	}
	args = append(args, arg...)
	prog, err := filepath.Abs(prog)
	fatalIf(t, err)
	if _, err := os.Stat(prog); err == nil {
		cmd = exec.Command(prog, args...)
	} else if _, err := os.Stat(prog + ".go"); err == nil {
		args = append([]string{"run", prog + ".go"}, args...)
		cmd = exec.Command("go", args...)
	} else {
		t.Fatalf("Cannot find binary or source for %s", prog)
	}
	cmd.Stderr = os.Stderr
	return cmd
}

// Run an example Go program, return the combined output as a string.
func runExample(t *testing.T, prog string, arg ...string) (string, error) {
	cmd := exampleCommand(t, prog, arg...)
	out, err := cmd.Output()
	return string(out), err
}

func prefix(prefix string, err error) error {
	if err != nil {
		return fmt.Errorf("%s: %s", prefix, err)
	}
	return nil
}

func runExampleWant(t *testing.T, want string, prog string, args ...string) error {
	out, err := runExample(t, prog, args...)
	if err != nil {
		return fmt.Errorf("%s failed: %s: %s", prog, err, out)
	}
	return prefix(prog, checkEqual(want, out))
}

func exampleArgs(args ...string) []string {
	for i := 0; i < *connections; i++ {
		args = append(args, fmt.Sprintf("%s/%s%d", testBroker.addr, "q", i))
	}
	return args
}

// Send then receive
func TestExampleSendReceive(t *testing.T) {
	if testing.Short() {
		t.Skip("Skip demo tests in short mode")
	}
	testBroker.start(t)
	err := runExampleWant(t,
		fmt.Sprintf("Received all %d acknowledgements\n", expected),
		"send",
		exampleArgs("-count", fmt.Sprintf("%d", *count))...)
	if err != nil {
		t.Fatal(err)
	}
	err = runExampleWant(t,
		fmt.Sprintf("Listening on %v connections\nReceived %v messages\n", *connections, *count**connections),
		"receive",
		exampleArgs("-count", fmt.Sprintf("%d", *count**connections))...)
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
func goReceiveWant(t *testing.T, errchan chan<- error, want string, arg ...string) *exec.Cmd {
	cmd := exampleCommand(t, "receive", arg...)
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
		listening := "Listening on 3 connections\n"
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
	testBroker.start(t)
	recvErr := make(chan error)
	recvCmd := goReceiveWant(
		t, recvErr,
		fmt.Sprintf("Received %d messages\n", expected),
		exampleArgs(fmt.Sprintf("-count=%d", expected))...)
	defer func() {
		recvCmd.Process.Kill()
		recvCmd.Wait()
	}()
	if err := <-recvErr; err != ready { // Wait for receiver ready
		t.Fatal(err)
	}
	err := runExampleWant(t,
		fmt.Sprintf("Received all %d acknowledgements\n", expected),
		"send",
		exampleArgs("-count", fmt.Sprintf("%d", *count))...)
	if err != nil {
		t.Fatal(err)
	}
	if err := <-recvErr; err != nil {
		t.Fatal(err)
	}
}

var testBroker *broker

var debug = flag.Bool("debug", false, "Debugging output from examples")
var brokerName = flag.String("broker", "broker", "Name of broker executable to run")
var count = flag.Int("count", 3, "Count of messages to send in tests")
var connections = flag.Int("connections", 3, "Number of connections to make in tests")
var expected int

func TestMain(m *testing.M) {
	expected = (*count) * (*connections)
	rand.Seed(time.Now().UTC().UnixNano())
	testBroker = &broker{} // Broker is started on-demand by tests.
	status := m.Run()
	testBroker.stop()
	os.Exit(status)
}
