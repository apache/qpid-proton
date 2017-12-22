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

/*
Package amqp encodes and decodes AMQP 1.0 messages and data types as Go types.

It follows the standard 'encoding' libraries pattern. The mapping between AMQP
and Go types is described in the documentation of the Marshal and Unmarshal
functions.

This package requires the [proton-C library](http://qpid.apache.org/proton) to be installed.

Package 'electron' is a full AMQP 1.0 client/server toolkit using this package.

AMQP 1.0 is an open standard for inter-operable message exchange, see <http://www.amqp.org/>
*/
package amqp

// #cgo LDFLAGS: -lqpid-proton-core
import "C"

// This file is just for the package comment.
