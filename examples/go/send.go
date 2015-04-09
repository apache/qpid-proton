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
)

// Simplistic error handling for demo. Not recommended.
func panicIf(err error) {
	if err != nil {
		panic(err)
	}
}

// Command-line flags
var count = flag.Int64("count", 0, "Send this may messages per address. 0 means unlimited.")

func main() {
	// Parse flags and arguments, print usage message on error.
	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, `
Usage: %s url [url ...]
Send messages to all the listed URLs concurrently.
To each URL, send the string "path-n" where n is the message number.
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
	if *count == 0 {
		*count = math.MaxInt64
	}

	// Create a goroutine for each URL that sends messages.
	var wait sync.WaitGroup // Used by main() to wait for all goroutines to end.
	wait.Add(len(urls))     // Wait for one goroutine per URL.

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
			// pc.Sender() is equivalent to pc.DefaultSession().Sender()
			s, err := pc.Sender(url.Path)
			panicIf(err)

			for i := int64(0); i < *count; i++ {
				m := proton.NewMessage()
				m.SetBody(fmt.Sprintf("%v-%v", url.Path, i))
				err := s.Send(m)
				panicIf(err)
			}
		}(urlStr)
	}
	wait.Wait() // Wait for all goroutines to finish.
}
