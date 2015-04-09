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
	"math"
	"net"
	"os"
	"qpid.apache.org/proton"
	"sync"
	"time"
)

// Simplistic error handling for demo. Not recommended.
func panicIf(err error) {
	if err != nil {
		panic(err)
	}
}

// Command-line flags
var count = flag.Int64("count", 0, "Stop after receiving this many messages. 0 means unlimited.")
var timeout = flag.Int64("time", 0, "Stop after this many seconds. 0 means unlimited.")
var short = flag.Bool("short", false, "Short format of message: body only")

func main() {
	// Parse flags and arguments, print usage message on error.
	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, `
Usage: %s url [url ...]
Receive messages from all the listed URLs concurrently and print them.
`, os.Args[0])
		flag.PrintDefaults()
	}
	flag.Parse()
	urls := flag.Args() // Non-flag arguments are URLs to receive from
	if len(urls) == 0 {
		flag.Usage()
		fmt.Fprintf(os.Stderr, "No URL provided")
		os.Exit(1)
	}
	duration := time.Duration(*timeout) * time.Second
	if duration == 0 {
		duration = time.Duration(math.MaxInt64) // Not forever, but 290 years is close enough.
	}
	if *count == 0 {
		*count = math.MaxInt64
	}

	// Create a goroutine for each URL that receives messages and sends them to
	// the messages channel. main() receives and prints them.

	messages := make(chan proton.Message) // Channel for messages from goroutines to main()
	stop := make(chan struct{})           // Closing this channel means the program is stopping.

	var wait sync.WaitGroup // Used by main() to wait for all goroutines to end.

	wait.Add(len(urls)) // Wait for one goroutine per URL.

	for _, urlStr := range urls {
		// Start a goroutine to receive from urlStr
		go func(urlStr string) {
			defer wait.Done()                   // Notify main() that this goroutine is done.
			url, err := proton.ParseURL(urlStr) // Like net/url.Parse() but with AMQP defaults.
			panicIf(err)

			// Open a standard Go net.Conn for the AMQP connection
			conn, err := net.Dial("tcp", url.Host) // Note net.URL.Host is actually "host:port"
			panicIf(err)
			defer conn.Close() // Close conn on goroutine exit.

			pc, err := proton.Connect(conn) // This is our AMQP connection.
			panicIf(err)
			// We could 'defer pc.Close()' but conn.close() will automatically close the proton connection.

			// For convenience a proton.Connection provides a DefaultSession()
			// pc.Receiver() is equivalent to pc.DefaultSession().Receiver()
			r, err := pc.Receiver(url.Path)
			panicIf(err)

			for m := range r.Receive { // r.Receive is a channel to receive messages.
				select {
				case messages <- m: // Send m to main()
				case <-stop: // The program is stopping.
					return
				}
			}
		}(urlStr)
	}

	// time.After() returns a channel that will close when the timeout is up.
	timer := time.After(duration)

	// main() prints each message and checks for count or timeout being exceeded.
	for i := *count; i > 0; i-- {
		select {
		case m := <-messages:
			if *short {
				fmt.Println(m.Body())
			} else {
				fmt.Printf("%#v\n\n", m)
			}
		case <-timer: // Timeout has expired
			i = 0
		}
	}

	close(stop) // Signal all goroutines to stop.
	wait.Wait() // Wait for all goroutines to finish.
}
