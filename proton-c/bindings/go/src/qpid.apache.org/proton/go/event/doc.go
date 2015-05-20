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
Package event provides a low-level API to the proton AMQP engine.

For most tasks, consider instead package qpid.apache.org/proton/go/messaging.
It provides a higher-level, concurrent API that is easier to use.

The API is event based. There are two alternative styles of handler. EventHandler
provides the core proton events. MessagingHandler provides a slighly simplified
view of the event stream and automates some common tasks.

See type Pump documentation for more details of the interaction between proton
events and goroutines.
*/
package event

// #cgo LDFLAGS: -lqpid-proton
import "C"

// This file is just for the package comment.
