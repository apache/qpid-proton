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
		":1234",
		"host/path",
		"amqps://host",
		"",
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
	// amqp://:1234
	// amqp://host:amqp/path
	// amqps://host:amqps
	// bad URL ""
}
