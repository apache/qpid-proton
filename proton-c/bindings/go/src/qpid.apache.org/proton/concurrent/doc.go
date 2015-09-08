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

Package concurrent provides a procedural, concurrent Go API for exchanging AMQP
messages.

AMPQ defines a credit-based scheme for flow control of messages over a
link. Credit is the number of messages the receiver is willing to accept.  The
receiver gives credit to the sender. The sender can send messages without
waiting for a response from the receiver until it runs out of credit, at which
point it must wait for more credit to send more messages.

See the documentation of Sender and Receiver for details of how this API uses credit.
*/
package concurrent

//#cgo LDFLAGS: -lqpid-proton
import "C"

// Just for package comment
