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

package amqp

import (
	"fmt"
)

func ExampleParseURL() {
	for _, s := range []string{
		"amqp://username:password@host:1234/path",
		"host:1234",
		"host",
		"host/path",
		"amqps://host",
		"/path",
		"",
		":1234",
                // Taken out because the go 1.4 URL parser isn't the same as later
		//"[::1]",
		//"[::1",
		// Output would be:
		// amqp://[::1]:amqp
		// parse amqp://[::1: missing ']' in host
	} {
		u, err := ParseURL(s)
		if err != nil {
			fmt.Println(err)
		} else {
			fmt.Println(u)
		}
	}
	// Output:
	// amqp://username:password@host:1234/path
	// amqp://host:1234
	// amqp://host:amqp
	// amqp://host:amqp/path
	// amqps://host:amqps
	// amqp://localhost:amqp/path
	// amqp://localhost:amqp
	// parse :1234: missing protocol scheme
}
