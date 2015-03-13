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
type Receiver struct{}

// OnMessage is called when an AMQP message is received.
func (r *Receiver) OnMessage(e *proton.Event) {
	fmt.Printf("%#v\n", e.Message)
}

var addr = flag.String("addr", ":amqp", "Listening address, e.g. localhost:1234")

func main() {
	flag.Usage = func() {
		fmt.Fprintln(os.Stderr, "Listen for AMQP messages and print them to stdout until killed.")
		flag.PrintDefaults()
	}
	flag.Parse()

	// Listen for and accept connections using the standard Go net package.
	listener, err := net.Listen("tcp", *addr)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Listen error: %s\n", err)
		os.Exit(1)
	}
	for {
		conn, err := listener.Accept()
		if err != nil {
			fmt.Fprintf(os.Stderr, "Accept error: %s\n", err)
			continue
		}
		fmt.Printf("Accepted connection %s<-%s\n", conn.LocalAddr(), conn.RemoteAddr())
		// Run processes an AMQP connection and invokes the supplied handler.
		// In this case it will call (*Receiver) OnMessage when a message is received.
		go proton.Run(conn, &Receiver{})
	}
}
