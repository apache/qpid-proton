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
	"qpid.apache.org/amqp"
	"qpid.apache.org/electron"
	"sync"
)

// Usage and command-line flags
func usage() {
	fmt.Fprintf(os.Stderr, `Usage: %s url [url ...]
Receive messages from all the listed URLs concurrently and print them.
`, os.Args[0])
	flag.PrintDefaults()
}

var count = flag.Uint64("count", 1, "Stop after receiving this many messages.")
var debug = flag.Bool("debug", false, "Print detailed debug output")
var debugf = func(format string, data ...interface{}) {} // Default no debugging output

func main() {
	flag.Usage = usage
	flag.Parse()

	if *debug {
		debugf = func(format string, data ...interface{}) { log.Printf(format, data...) }
	}
	urls := flag.Args() // Non-flag arguments are URLs to receive from
	if len(urls) == 0 {
		log.Println("No URL provided")
		usage()
		os.Exit(1)
	}

	messages := make(chan amqp.Message) // Channel for messages from goroutines to main()
	defer close(messages)

	var wait sync.WaitGroup // Used by main() to wait for all goroutines to end.
	wait.Add(len(urls))     // Wait for one goroutine per URL.

	container := electron.NewContainer(fmt.Sprintf("receive[%v]", os.Getpid()))
	connections := make(chan electron.Connection, len(urls)) // Connections to close on exit

	// Start a goroutine to for each URL to receive messages and send them to the messages channel.
	// main() receives and prints them.
	for _, urlStr := range urls {
		debugf("Connecting to %s\n", urlStr)
		go func(urlStr string) { // Start the goroutine
			defer wait.Done() // Notify main() when this goroutine is done.
			url, err := amqp.ParseURL(urlStr)
			fatalIf(err)
			c, err := container.Dial("tcp", url.Host)
			fatalIf(err)
			connections <- c // Save connection so we can Close() when main() ends
			r, err := c.Receiver(electron.Source(url.Path))
			fatalIf(err)
			// Loop receiving messages and sending them to the main() goroutine
			for {
				if rm, err := r.Receive(); err == nil {
					rm.Accept()
					messages <- rm.Message
				} else if err == electron.Closed {
					return
				} else {
					log.Fatal("receive error %v: %v", urlStr, err)
				}
			}
		}(urlStr)
	}

	// All goroutines are started, we are receiving messages.
	fmt.Printf("Listening on %d connections\n", len(urls))

	// print each message until the count is exceeded.
	for i := uint64(0); i < *count; i++ {
		m := <-messages
		debugf("%v\n", m.Body())
	}
	fmt.Printf("Received %d messages\n", *count)

	// Close all connections, this will interrupt goroutines blocked in Receiver.Receive()
	// with electron.Closed.
	for i := 0; i < len(urls); i++ {
		c := <-connections
		debugf("close %s", c)
		c.Close(nil)
	}
	wait.Wait() // Wait for all goroutines to finish.
}

func fatalIf(err error) {
	if err != nil {
		log.Fatal(err)
	}
}
