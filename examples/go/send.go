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
Send messages to each URL concurrently with body "<url-path>-<n>" where n is the message number.
`, os.Args[0])
	flag.PrintDefaults()
}

var debug = flag.Bool("debug", false, "Print detailed debug output")
var count = flag.Int64("count", 1, "Send this may messages per address.")

// Ack associates an info string with an acknowledgement
type Ack struct {
	ack  messaging.Acknowledgement
	info string
}

func main() {
	flag.Usage = usage
	flag.Parse()

	urls := flag.Args() // Non-flag arguments are URLs to receive from
	if len(urls) == 0 {
		fmt.Fprintln(os.Stderr, "No URL provided")
		flag.Usage()
		os.Exit(1)
	}

	acks := make(chan Ack)  // Channel to receive all the acknowledgements
	var wait sync.WaitGroup // Used by main() to wait for all goroutines to end.
	wait.Add(len(urls))     // Wait for one goroutine per URL.

	connections := make([]*messaging.Connection, len(urls)) // Store connctions to close on exit

	// Start a goroutine for each URL to send messages, receive the acknowledgements and
	// send them to the acks channel.
	for i, urlStr := range urls {
		debugf("Connecting to %v\n", urlStr)
		go func(urlStr string) {

			defer wait.Done()                 // Notify main() that this goroutine is done.
			url, err := amqp.ParseURL(urlStr) // Like net/url.Parse() but with AMQP defaults.
			exitIf(err)

			// Open a standard Go net.Conn and and AMQP connection using it.
			conn, err := net.Dial("tcp", url.Host) // Note net.URL.Host is actually "host:port"
			exitIf(err)
			pc, err := messaging.Connect(conn) // This is our AMQP connection.
			exitIf(err)
			connections[i] = pc // Save connection so it will be closed when main() ends

			// Create a sender using the path of the URL as the AMQP address
			s, err := pc.Sender(url.Path)
			exitIf(err)

			// Loop sending messages, receiving acknowledgements and sending them to the acks channel.
			for i := int64(0); i < *count; i++ {
				m := amqp.NewMessage()
				body := fmt.Sprintf("%v-%v", url.Path, i)
				m.SetBody(body)
				// Note Send is *asynchronous*, ack is a channel that will receive the acknowledgement.
				ack, err := s.Send(m)
				exitIf(err)
				acks <- Ack{ack, body} // Send the acknowledgement to main()
			}
		}(urlStr)
	}

	// Wait for all the acknowledgements
	expect := int(*count) * len(urls)
	debugf("Started senders, expect %v acknowledgements\n", expect)
	for i := 0; i < expect; i++ {
		ack, ok := <-acks
		if !ok {
			exitIf(fmt.Errorf("acks channel closed after only %d acks\n", i))
		}
		d := <-ack.ack
		debugf("acknowledgement[%v] %v\n", i, ack.info)
		if d != messaging.Accepted {
			fmt.Printf("Unexpected disposition %v\n", d)
		}
	}
	fmt.Printf("Received all %v acknowledgements\n", expect)

	wait.Wait()                     // Wait for all goroutines to finish.
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
