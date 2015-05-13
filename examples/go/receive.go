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
	"io"
	"io/ioutil"
	"log"
	"math"
	"net"
	"os"
	"path"
	"qpid.apache.org/proton"
	"qpid.apache.org/proton/messaging"
	"sync"
	"time"
)

// Command-line flags
var verbose = flag.Int("verbose", 1, "Output level, 0 means none, higher means more")
var count = flag.Int64("count", 0, "Stop after receiving this many messages. 0 means unlimited.")
var timeout = flag.Int64("time", 0, "Stop after this many seconds. 0 means unlimited.")
var full = flag.Bool("full", false, "Print full message not just body.")

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

	// Arrange to close all connections on exit
	connections := make([]*messaging.Connection, len(urls))
	defer func() {
		for _, c := range connections {
			if c != nil {
				c.Close()
			}
		}
	}()

	for i, urlStr := range urls {
		debug.Printf("Connecting to %s", urlStr)
		go func(urlStr string) {
			defer wait.Done()                   // Notify main() that this goroutine is done.
			url, err := proton.ParseURL(urlStr) // Like net/url.Parse() but with AMQP defaults.
			fatalIf(err)

			// Open a standard Go net.Conn for the AMQP connection
			conn, err := net.Dial("tcp", url.Host) // Note net.URL.Host is actually "host:port"
			fatalIf(err)

			pc, err := messaging.Connect(conn) // This is our AMQP connection.
			fatalIf(err)
			connections[i] = pc

			// For convenience a proton.Connection provides a DefaultSession()
			// pc.Receiver() is equivalent to pc.DefaultSession().Receiver()
			r, err := pc.Receiver(url.Path)
			fatalIf(err)

			for {
				var m proton.Message
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
	info.Printf("Listening")

	// time.After() returns a channel that will close when the timeout is up.
	timer := time.After(duration)

	// main() prints each message and checks for count or timeout being exceeded.
	for i := int64(0); i < *count; i++ {
		select {
		case m := <-messages:
			debug.Print(formatMessage{m})
		case <-timer: // Timeout has expired
			i = 0
		}
	}
	info.Printf("Received %d messages", *count)
	close(stop) // Signal all goroutines to stop.
	wait.Wait() // Wait for all goroutines to finish.
}

// Logging
func logger(prefix string, level int, w io.Writer) *log.Logger {
	if *verbose >= level {
		return log.New(w, prefix, 0)
	}
	return log.New(ioutil.Discard, "", 0)
}

var info, debug *log.Logger

func init() {
	flag.Parse()
	name := path.Base(os.Args[0])
	log.SetFlags(0)                                               // Use default logger for errors.
	log.SetPrefix(fmt.Sprintf("%s: ", name))                      // Log errors on stderr.
	info = logger(fmt.Sprintf("%s: ", name), 1, os.Stdout)        // Log info on stdout.
	debug = logger(fmt.Sprintf("%s debug: ", name), 2, os.Stderr) // Log debug on stderr.
}

// Simple error handling for demo.
func fatalIf(err error) {
	if err != nil {
		log.Fatal(err)
	}
}

type formatMessage struct{ m proton.Message }

func (fm formatMessage) String() string {
	if *full {
		return fmt.Sprintf("%#v", fm.m)
	} else {
		return fmt.Sprintf("%#v", fm.m.Body())
	}
}
