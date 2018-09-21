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

package electron

import (
	"fmt"
	"net"
	"path"
	"reflect"
	"runtime"
	"testing"
)

func decorate(err error, callDepth int) string {
	_, file, line, _ := runtime.Caller(callDepth + 1) // annotate with location of caller.
	_, file = path.Split(file)
	return fmt.Sprintf("\n%s:%d: %v", file, line, err)
}

func fatalIfN(t testing.TB, err error, callDepth int) {
	if err != nil {
		t.Fatal(decorate(err, callDepth+1))
	}
}

func fatalIf(t testing.TB, err error) {
	fatalIfN(t, err, 1)
}

func errorIf(t testing.TB, err error) {
	if err != nil {
		t.Errorf(decorate(err, 1))
	}
}

func checkEqual(want interface{}, got interface{}) error {
	if !reflect.DeepEqual(want, got) {
		return fmt.Errorf("(%#v != %#v)", want, got)
	}
	return nil
}

// AMQP client/server pair
type pair struct {
	t        testing.TB
	client   Session
	server   Connection
	capacity int
	prefetch bool
	rchan    chan Receiver
	schan    chan Sender
	auth     connectionSettings
}

func newPair(t testing.TB, cli, srv net.Conn, clientOpts, serverOpts []ConnectionOption) *pair {
	opts := append([]ConnectionOption{Server()}, serverOpts...)
	sc, _ := NewConnection(srv, opts...)
	opts = append([]ConnectionOption{}, clientOpts...)
	cc, _ := NewConnection(cli, opts...)
	cs, _ := cc.Session()
	p := &pair{
		t:        t,
		client:   cs,
		server:   sc,
		capacity: 100,
		rchan:    make(chan Receiver),
		schan:    make(chan Sender)}

	go func() {
		for i := range p.server.Incoming() {
			switch i := i.(type) {
			case *IncomingReceiver:
				if p.capacity > 0 {
					i.SetCapacity(p.capacity)
				}
				i.SetPrefetch(p.prefetch)
				p.rchan <- i.Accept().(Receiver)
				break
			case *IncomingSender:
				p.schan <- i.Accept().(Sender)
			default:
				i.Accept()
			}
		}
	}()

	return p
}

// AMQP pair linked by in-memory pipe
func newPipe(t testing.TB, clientOpts, serverOpts []ConnectionOption) *pair {
	cli, srv := net.Pipe()
	return newPair(t, cli, srv, clientOpts, serverOpts)
}

// AMQP pair linked by TCP socket
func newSocketPair(t testing.TB, clientOpts, serverOpts []ConnectionOption) *pair {
	l, err := net.Listen("tcp4", ":0") // For systems with ipv6 disabled
	fatalIfN(t, err, 1)
	srvCh := make(chan net.Conn)
	var srvErr error
	go func() {
		var c net.Conn
		c, srvErr = l.Accept()
		srvCh <- c
	}()
	addr := l.Addr()
	cli, err := net.Dial(addr.Network(), addr.String())
	fatalIfN(t, err, 1)
	srv := <-srvCh
	fatalIfN(t, srvErr, 1)
	return newPair(t, cli, srv, clientOpts, serverOpts)
}

func (p *pair) close() { p.client.Connection().Close(nil); p.server.Close(nil) }

// Return a client sender and server receiver
func (p *pair) sender(opts ...LinkOption) (Sender, Receiver) {
	snd, err := p.client.Sender(opts...)
	fatalIfN(p.t, err, 2)
	rcv := <-p.rchan
	return snd, rcv
}

// Return a client receiver and server sender
func (p *pair) receiver(opts ...LinkOption) (Receiver, Sender) {
	rcv, err := p.client.Receiver(opts...)
	fatalIfN(p.t, err, 2)
	snd := <-p.schan
	return rcv, snd
}
