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
	"log"
	"os"
	"strings"

	"github.com/apache/qpid-proton/go/pkg/amqp"
	"github.com/apache/qpid-proton/go/pkg/electron"
)

// NOTE: this example requires a broker thta supports the APACHE.ORG:SELECTOR filter.

// Usage and command-line flags
func usage() {
	fmt.Fprintf(os.Stderr, `Usage: %s
Receive messages that match a query.
`, os.Args[0])
	flag.PrintDefaults()
}

var count = flag.Int("count", 5, "Number of messages to send, will only receive half")
var url = flag.String("url", "/examples", "Receive messages from this URL")

// Example function to create an AMQP filter-set containing an APACHE.ORG:SELECTOR filter.
// This filters messages using an SQL-like query expression.
// See http://www.amqp.org/specification/1.0/filters for more
func selectorFilter(query string) map[amqp.Symbol]interface{} {
	filter := amqp.Described{
		Descriptor: amqp.Symbol("apache.org:selector-filter:string"), // Identify the type of filter
		Value:      query,                                            // Query to evaluate
	}
	// Return a filter set (map) with a single selector filter.
	// Note "selector" is an arbitrary name to identify the filter, it can be any non-empty string.
	return map[amqp.Symbol]interface{}{"selector": filter}
}

func main() {
	flag.Usage = usage
	flag.Parse()
	u, err := amqp.ParseURL(*url)
	fatalIf(err)
	container := electron.NewContainer(fmt.Sprintf("%s[%v]", os.Args[0], os.Getpid()))
	c, err := container.Dial("tcp", u.Host) // NOTE: Dial takes just the Host part of the U
	fatalIf(err)
	defer c.Close(nil)
	addr := strings.TrimPrefix(u.Path, "/")
	s, err := c.Sender(electron.Target(addr))
	fatalIf(err)
	r, err := c.Receiver(electron.Source(addr), electron.Filter(selectorFilter("colour = 'green'")))
	fatalIf(err)

	send(s, *count)
	receive(r, *count/2)
}

func send(s electron.Sender, n int) {
	defer s.Close(nil)
	for i := 0; i < n; i++ {
		m := amqp.NewMessageWith(i)
		if i%2 == 0 {
			m.ApplicationProperties()["colour"] = "red"
		} else {
			m.ApplicationProperties()["colour"] = "green"
		}
		s.SendSync(m)
	}
}

func receive(r electron.Receiver, n int) {
	defer r.Close(nil)
	for i := 0; i < n; i++ {
		switch rm, err := r.Receive(); err {
		case nil:
			rm.Accept()
			log.Print(rm.Message)
		case electron.Closed:
			return
		default:
			log.Fatalf("receive error: %v", err)
		}
	}
}

func fatalIf(err error) {
	if err != nil {
		log.Fatal(err)
	}
}
