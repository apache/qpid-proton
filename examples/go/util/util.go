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

// util contains utility types and functions to simplify parts of the example
// code that are not related to the use of proton.
package util

import (
	"flag"
	"fmt"
	"log"
	"os"
	"path"
	"qpid.apache.org/amqp"
)

// Debug flag "-debug" enables debug output with Debugf
var Debug = flag.Bool("debug", false, "Print detailed debug output")

// Full flag "-full" enables full message output by FormatMessage
var Full = flag.Bool("full", false, "Print full message not just body.")

// Debugf logs debug messages if "-debug" flag is set.
func Debugf(format string, data ...interface{}) {
	if *Debug {
		log.Printf(format, data...)
	}
}

// Simple error handling for demo.
func ExitIf(err error) {
	if err != nil {
		log.Fatal(err)
	}
}

// FormatMessage formats a message as a string, just the body by default or
// the full message (with properties etc.) if "-full" flag is set.
func FormatMessage(m amqp.Message) string {
	if *Full {
		return fmt.Sprintf("%#v", m)
	} else {
		return fmt.Sprintf("%#v", m.Body())
	}
}

// For example programs, use the program name as the log prefix.
func init() {
	log.SetFlags(0)
	_, prog := path.Split(os.Args[0])
	log.SetPrefix(fmt.Sprintf("%s(%d): ", prog, os.Getpid()))
}
