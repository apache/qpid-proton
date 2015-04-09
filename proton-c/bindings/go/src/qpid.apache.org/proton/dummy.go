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

package proton

import (
	"fmt"
	"net"
)

// Placeholder definitions to allow examples to compile.

type Connection struct {
	Server bool // Server connection does protocol negotiation
	// FIXME aconway 2015-04-17: Other parameters to set up SSL, SASL etc.
}

// Map an AMQP connection using conn
func (c Connection) Connect(conn net.Conn) error { return nil }
func (c Connection) Close() error                { return nil }

func (c Connection) Receiver(addr string) (*Receiver, error) {
	// FIXME aconway 2015-04-10: dummy implementation to test examples, returns endless messages.
	r := &Receiver{make(chan Message), make(chan struct{})}
	go func() {
		for i := 0; ; i++ {
			m := NewMessage()
			m.SetBody(fmt.Sprintf("%v-%v", addr, i))
			select {
			case r.Receive <- m:
			case <-r.closed:
				return
			}
		}
	}()
	return r, nil
}

func (c Connection) Sender(addr string) (*Sender, error) {
	return &Sender{}, nil
}

type Receiver struct {
	Receive chan Message
	closed  chan struct{}
}

func (r Receiver) Close() error { return nil }

type Sender struct{}

func (s Sender) Send(m Message) error { fmt.Println(m.Body()); return nil }
func (s Sender) Close() error         { return nil }

// Connect makes a default client connection using conn.
//
// For more control do:
//     c := Connection{}
//     // set parameters on c
//     c.Connect(conn)
//
func Connect(conn net.Conn) (Connection, error) {
	c := Connection{}
	c.Connect(conn)
	return c, nil
}
