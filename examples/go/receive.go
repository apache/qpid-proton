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
	"qpid.apache.org/proton/go/amqp"
	"qpid.apache.org/proton/go/messaging"
	"sync"
)

// Usage and command-line flags
func usage() {
	fmt.Fprintf(os.Stderr, `Usage: %s url [url ...]
Receive messages from all the listed URLs concurrently and print them.
`, os.Args[0])
	flag.PrintDefaults()
}

var debug = flag.Bool("debug", false, "Print detailed debug output")
var count = flag.Uint64("count", 1, "Stop after receiving this many messages.")
var full = flag.Bool("full", false, "Print full message not just body.")

func main() {
	flag.Usage = usage
	flag.Parse()

	urls := flag.Args() // Non-flag arguments are URLs to receive from
	if len(urls) == 0 {
		fmt.Fprintln(os.Stderr, "No URL provided")
		usage()
		os.Exit(1)
	}

	messages := make(chan amqp.Message) // Channel for messages from goroutines to main()
	stop := make(chan struct{})         // Closing this channel means the program is stopping.
	var wait sync.WaitGroup             // Used by main() to wait for all goroutines to end.
	wait.Add(len(urls))                 // Wait for one goroutine per URL.

	connections := make([]*messaging.Connection, len(urls)) // Store connctions to close on exit

	// Start a goroutine to for each URL to receive messages and send them to the messages channel.
	// main() receives and prints them.
	for i, urlStr := range urls {
		debugf("debug: Connecting to %s\n", urlStr)
		go func(urlStr string) { // Start the goroutine

			defer wait.Done()                 // Notify main() when this goroutine is done.
			url, err := amqp.ParseURL(urlStr) // Like net/url.Parse() but with AMQP defaults.
			exitIf(err)

			// Open a standard Go net.Conn and and AMQP connection using it.
			conn, err := net.Dial("tcp", url.Host) // Note net.URL.Host is actually "host:port"
			exitIf(err)
			pc, err := messaging.Connect(conn) // This is our AMQP connection.
			exitIf(err)
			connections[i] = pc // Save connection so it will be closed when main() ends

			// Create a receiver using the path of the URL as the AMQP address
			r, err := pc.Receiver(url.Path)
			exitIf(err)

			// Loop receiving messages
			for {
				var m amqp.Message
				select { // Receive a message or stop.
				case m = <-r.Receive:
				case <-stop: // The program is stopping.
					return
				}
				select { // Send m to main() or stop
				case messages <- m: // Send m to main()
				case <-stop: // The program is stopping.
					return
				}
			}
		}(urlStr)
	}

	// All goroutines are started, we are receiving messages.
	fmt.Printf("Listening on %d connections\n", len(urls))

	// print each message until the count is exceeded.
	for i := uint64(0); i < *count; i++ {
		debugf("%s\n", formatMessage(<-messages))
	}
	fmt.Printf("Received %d messages\n", *count)
	close(stop) // Signal all goroutines to stop.
	wait.Wait() // Wait for all goroutines to finish.
	close(messages)
	for _, c := range connections { // Close all connections
		if c != nil {
			c.Close()
		}
	}
}

// Simple debug logging
func debugf(format string, data ...interface{}) {
	if *debug {
		fmt.Fprintf(os.Stderr, format, data...)
	}
}

// Simple error handling for demo.
func exitIf(err error) {
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
	}
}

func formatMessage(m amqp.Message) string {
	if *full {
		return fmt.Sprintf("%#v", m)
	} else {
		return fmt.Sprintf("%#v", m.Body())
	}
}
