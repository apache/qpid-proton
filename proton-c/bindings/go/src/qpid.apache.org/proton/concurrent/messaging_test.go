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

package concurrent

import (
	"fmt"
	"net"
	"qpid.apache.org/proton/amqp"
	"testing"
	"time"
)

func panicIf(err error) {
	if err != nil {
		panic(err)
	}
}

// Start a server, return listening addr and channel for incoming Connection.
func newServer(cont Container) (net.Addr, <-chan Connection) {
	listener, err := net.Listen("tcp", "")
	panicIf(err)
	addr := listener.Addr()
	ch := make(chan Connection)
	go func() {
		conn, err := listener.Accept()
		c, err := cont.Connection(conn)
		panicIf(err)
		c.Server()
		c.Listen()
		panicIf(c.Open())
		ch <- c
	}()
	return addr, ch
}

// Return open an client connection and session, return the session.
func newClient(cont Container, addr net.Addr) Session {
	conn, err := net.Dial(addr.Network(), addr.String())
	panicIf(err)
	c, err := cont.Connection(conn)
	panicIf(err)
	c.Open()
	sn, err := c.Session()
	panicIf(err)
	panicIf(sn.Open())
	return sn
}

// Return client and server ends of the same connection.
func newClientServer() (client Session, server Connection) {
	addr, ch := newServer(NewContainer(""))
	client = newClient(NewContainer(""), addr)
	return client, <-ch
}

// Close client and server
func closeClientServer(client Session, server Connection) {
	client.Connection().Close(nil)
	server.Close(nil)
}

// Send a message one way with a client sender and server receiver, verify ack.
func TestClientSendServerReceive(t *testing.T) {
	var err error
	client, server := newClientServer()
	defer func() {
		closeClientServer(client, server)
	}()

	timeout := time.Second * 2
	nLinks := 3
	nMessages := 3

	s := make([]Sender, nLinks)
	for i := 0; i < nLinks; i++ {
		s[i], err = client.Sender(fmt.Sprintf("foo%d", i))
		if err != nil {
			t.Fatal(err)
		}
	}

	// Server accept session and receivers
	ep, err := server.Accept()
	ep.Open() // Accept incoming session
	r := make([]Receiver, nLinks)
	for i := 0; i < nLinks; i++ { // Accept incoming receivers
		ep, err = server.Accept()
		r[i] = ep.(Receiver)
		err = r[i].Open()
		if err != nil {
			t.Fatal(err)
		}
	}

	for i := 0; i < nLinks; i++ {
		for j := 0; j < nMessages; j++ {
			// Client send
			m := amqp.NewMessageWith(fmt.Sprintf("foobar%v-%v", i, j))
			sm, err := s[i].Send(m)
			if err != nil {
				t.Fatal(err)
			}

			// Server recieve
			rm, err := r[i].Receive()
			if err != nil {
				t.Fatal(err)
			}
			if want, got := interface{}(fmt.Sprintf("foobar%v-%v", i, j)), rm.Message.Body(); want != got {
				t.Errorf("%#v != %#v", want, got)
			}

			// Should not be acknowledged on client yet
			if d, err := sm.DispositionTimeout(0); err != Timeout || NoDisposition != d {
				t.Errorf("want [no-disposition/timeout] got [%v/%v]", d, err)
			}
			// Server ack
			if err := rm.Acknowledge(Rejected); err != nil {
				t.Error(err)
			}
			// Client get ack.
			if d, err := sm.DispositionTimeout(timeout); err != nil || Rejected != d {
				t.Errorf("want [rejected/nil] got [%v/%v]", d, err)
			}
		}
	}
}

func TestClientReceiver(t *testing.T) {
	client, server := newClientServer()
	nMessages := 3

	done := make(chan struct{})
	go func() { // Server sends
		defer close(done)
		for {
			ep, err := server.Accept()
			switch {
			case err == Closed:
				return
			case err == nil:
				break
			default:
				t.Error(err)
				return
			}
			ep.Open()
			if s, ok := ep.(Sender); ok {
				go func() {
					for i := int32(0); i < int32(nMessages); i++ {
						sm, err := s.Send(amqp.NewMessageWith(i))
						if err != nil {
							t.Error(err)
							return
						} else {
							sm.Disposition() // Sync send.
						}
					}
					s.Close(nil)
				}()
			}
		}
	}()

	r, err := client.Receiver("foo")
	if err != nil {
		t.Fatal(err)
	}
	for i := int32(0); i < int32(nMessages); i++ {
		rm, err := r.Receive()
		if err != nil {
			if err != Closed {
				t.Error(err)
			}
			break
		}
		if err := rm.Accept(); err != nil {
			t.Error(err)
		}
		if b, ok := rm.Message.Body().(int32); !ok || b != i {
			t.Errorf("want %v, true got %v, %v", i, b, ok)
		}
	}
	server.Close(nil)
	<-done
	client.Connection().Close(nil)
}
