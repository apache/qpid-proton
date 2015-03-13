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

package main

import (
	"flag"
	"fmt"
	"net"
	"os"
	"qpid.apache.org/proton"
)

// A message handler type
type Sender struct {
	Messages       []string
	Sent, Accepted int
	Sender         *proton.Sender
}

// Called when proton.Run is first called on a connection.
func (s *Sender) OnStart(e *proton.Event) {
	// Create a sender
	// FIXME aconway 2015-03-13: Creating sendders on other connections?
	s.Sender = e.Connection().NewSender("sender")
}

// Called when a sender has credit to send messages. Send all of flag.Args()
func (s *Sender) OnSendable(e *proton.Event) {
	for e.Sender().Credit() > 0 && s.Sent < len(flag.Args()) {
		// FIXME aconway 2015-03-13: error handling
		e.Sender().Send(&proton.Message{Body: s.Messages[s.Sent]})
		s.Sent++
	}
}

func (s *Sender) OnAccepted(e *proton.Event) {
	s.Accepted++
	if s.Accepted == len(flag.Args()) {
		e.Connection().Close()
	}
}

var addr = flag.String("addr", ":amqp", "Listening address, e.g. localhost:1234")

func main() {
	flag.Usage = func() {
		fmt.Fprintln(os.Stderr, "Send AMQP messages with string bodies, one for each argument.")
		flag.PrintDefaults()
	}
	flag.Parse()

	conn, err := net.Dial("tcp", *addr)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Dial error: %s\n", err)
		os.Exit(1)
	}
	// Run processes an AMQP connection and invokes the supplied handler.
	// In this case it will call OnStart.
	proton.Run(conn, &Sender{Messages: flag.Args()})
}
