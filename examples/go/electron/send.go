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
	"./util"
	"flag"
	"fmt"
	"log"
	"net"
	"os"
	"path"
	"qpid.apache.org/amqp"
	"qpid.apache.org/electron"
	"sync"
)

// Usage and command-line flags
func usage() {
	fmt.Fprintf(os.Stderr, `Usage: %s url [url ...]
Send messages to each URL concurrently with body "<url-path>-<n>" where n is the message number.
`, os.Args[0])
	flag.PrintDefaults()
}

var count = flag.Int64("count", 1, "Send this may messages per address.")

func main() {
	flag.Usage = usage
	flag.Parse()

	urls := flag.Args() // Non-flag arguments are URLs to receive from
	if len(urls) == 0 {
		log.Println("No URL provided")
		flag.Usage()
		os.Exit(1)
	}

	sentChan := make(chan electron.Outcome) // Channel to receive acknowledgements.

	var wait sync.WaitGroup
	wait.Add(len(urls)) // Wait for one goroutine per URL.

	_, prog := path.Split(os.Args[0])
	container := electron.NewContainer(fmt.Sprintf("%v:%v", prog, os.Getpid()))
	connections := make(chan electron.Connection, len(urls)) // Connctions to close on exit

	// Start a goroutine for each URL to send messages.
	for _, urlStr := range urls {
		util.Debugf("Connecting to %v\n", urlStr)
		go func(urlStr string) {

			defer wait.Done()                 // Notify main() that this goroutine is done.
			url, err := amqp.ParseURL(urlStr) // Like net/url.Parse() but with AMQP defaults.
			util.ExitIf(err)

			// Open a new connection
			conn, err := net.Dial("tcp", url.Host) // Note net.URL.Host is actually "host:port"
			util.ExitIf(err)
			c, err := container.Connection(conn)
			util.ExitIf(err)
			connections <- c // Save connection so we can Close() when main() ends

			// Create a Sender using the path of the URL as the AMQP address
			s, err := c.Sender(electron.Target(url.Path))
			util.ExitIf(err)

			// Loop sending messages.
			for i := int64(0); i < *count; i++ {
				m := amqp.NewMessage()
				body := fmt.Sprintf("%v-%v", url.Path, i)
				m.Marshal(body)
				s.SendAsync(m, sentChan, body) // Outcome will be sent to sentChan
			}
		}(urlStr)
	}

	// Wait for all the acknowledgements
	expect := int(*count) * len(urls)
	util.Debugf("Started senders, expect %v acknowledgements\n", expect)
	for i := 0; i < expect; i++ {
		out := <-sentChan // Outcome of async sends.
		if out.Error != nil {
			util.Debugf("acknowledgement[%v] %v error: %v\n", i, out.Value, out.Error)
		} else {
			util.Debugf("acknowledgement[%v]  %v (%v)\n", i, out.Value, out.Status)
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
