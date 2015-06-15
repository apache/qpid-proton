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
)

// Command-line flags
var verbose = flag.Int("verbose", 1, "Output level, 0 means none, higher means more")
var count = flag.Int64("count", 1, "Send this may messages per address. 0 means unlimited.")

// Ack associates an info string with an acknowledgement
type Ack struct {
	ack  messaging.Acknowledgement
	info string
}

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
		fmt.Fprintf(os.Stderr, "No URL provided\n")
		os.Exit(1)
	}
	if *count == 0 {
		*count = math.MaxInt64
	}

	// Create a channel to receive all the acknowledgements
	acks := make(chan Ack)

	// Create a goroutine for each URL that sends messages.
	var wait sync.WaitGroup // Used by main() to wait for all goroutines to end.
	wait.Add(len(urls))     // Wait for one goroutine per URL.

	// Arrange to close all connections on exit
	connections := make([]*messaging.Connection, len(urls))
	defer func() {
		for _, c := range connections {
			c.Close()
		}
	}()

	for i, urlStr := range urls {
		url, err := proton.ParseURL(urlStr) // Like net/url.Parse() but with AMQP defaults.
		fatalIf(err)
		debug.Printf("Connecting to %v", url)

		// Open a standard Go net.Conn for the AMQP connection
		conn, err := net.Dial("tcp", url.Host) // Note net.URL.Host is actually "host:port"
		fatalIf(err)

		pc, err := messaging.Connect(conn) // This is our AMQP connection using conn.
		fatalIf(err)
		connections[i] = pc

		// Start a goroutine to send to urlStr
		go func(urlStr string) {
			defer wait.Done() // Notify main() that this goroutine is done.

			// FIXME aconway 2015-04-29: sessions, default sessions, senders...
			// Create a sender using the path of the URL as the AMQP target address
			s, err := pc.Sender(url.Path)
			fatalIf(err)

			for i := int64(0); i < *count; i++ {
				m := proton.NewMessage()
				body := fmt.Sprintf("%v-%v", url.Path, i)
				m.SetBody(body)
				ack, err := s.Send(m)
				fatalIf(err)
				acks <- Ack{ack, body}
			}
		}(urlStr)
	}

	// Wait for all the acknowledgements
	expect := int(*count) * len(urls)
	debug.Printf("Started senders, expect %v acknowledgements", expect)
	for i := 0; i < expect; i++ {
		ack, ok := <-acks
		if !ok {
			info.Fatalf("acks channel closed after only %d acks\n", i)
		}
		d := <-ack.ack
		debug.Printf("acknowledgement[%v] %v", i, ack.info)
		if d != messaging.Accepted {
			info.Printf("Unexpected disposition %v", d)
		}
	}
	info.Printf("Received all %v acknowledgements", expect)
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
