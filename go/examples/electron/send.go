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
	"github.com/apache/qpid-proton/go/pkg/amqp"
	"github.com/apache/qpid-proton/go/pkg/electron"
	"strings"
	"sync"
)

// Usage and command-line flags
func usage() {
	fmt.Fprintf(os.Stderr, `Usage: %s url [url ...]
Send messages to each URL concurrently.
URLs are of the form "amqp://<host>:<port>/<amqp-address>"
`, os.Args[0])
	flag.PrintDefaults()
}

var count = flag.Int64("count", 1, "Send this many messages to each address.")
var debug = flag.Bool("debug", false, "Print detailed debug output")
var Debugf = func(format string, data ...interface{}) {} // Default no debugging output

func main() {
	flag.Usage = usage
	flag.Parse()

	if *debug {
		Debugf = func(format string, data ...interface{}) { log.Printf(format, data...) }
	}

	urls := flag.Args() // Non-flag arguments are URLs to receive from
	if len(urls) == 0 {
		log.Println("No URL provided")
		flag.Usage()
		os.Exit(1)
	}

	sentChan := make(chan electron.Outcome) // Channel to receive acknowledgements.

	var wait sync.WaitGroup
	wait.Add(len(urls)) // Wait for one goroutine per URL.

	container := electron.NewContainer(fmt.Sprintf("send[%v]", os.Getpid()))
	connections := make(chan electron.Connection, len(urls)) // Connections to close on exit

	// Start a goroutine for each URL to send messages.
	for _, urlStr := range urls {
		Debugf("Connecting to %v\n", urlStr)
		go func(urlStr string) {
			defer wait.Done() // Notify main() when this goroutine is done.
			url, err := amqp.ParseURL(urlStr)
			fatalIf(err)
			c, err := container.Dial("tcp", url.Host) // NOTE: Dial takes just the Host part of the URL
			fatalIf(err)
			connections <- c // Save connection so we can Close() when main() ends
			addr := strings.TrimPrefix(url.Path, "/")
			s, err := c.Sender(electron.Target(addr))
			fatalIf(err)
			// Loop sending messages.
			for i := int64(0); i < *count; i++ {
				m := amqp.NewMessage()
				body := fmt.Sprintf("%v%v", addr, i)
				m.Marshal(body)
				s.SendAsync(m, sentChan, body) // Outcome will be sent to sentChan
			}
		}(urlStr)
	}

	// Wait for all the acknowledgements
	expect := int(*count) * len(urls)
	Debugf("Started senders, expect %v acknowledgements\n", expect)
	for i := 0; i < expect; i++ {
		out := <-sentChan // Outcome of async sends.
		if out.Error != nil {
			log.Fatalf("acknowledgement[%v] %v error: %v", i, out.Value, out.Error)
		} else if out.Status != electron.Accepted {
			log.Fatalf("acknowledgement[%v] unexpected status: %v", i, out.Status)
		} else {
			Debugf("acknowledgement[%v]  %v (%v)\n", i, out.Value, out.Status)
		}
	}
	fmt.Printf("Received all %v acknowledgements\n", expect)

	wait.Wait() // Wait for all goroutines to finish.
	close(connections)
	for c := range connections { // Close all connections
		if c != nil {
			c.Close(nil)
		}
	}
}

func fatalIf(err error) {
	if err != nil {
		log.Fatal(err)
	}
}
