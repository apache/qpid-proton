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

package proton

import (
	"net"
)

type Acceptor struct{}

type Event struct {
	Container *Container
	Message   *Message
}

type Connection struct{}

func (e *Event) Connection() *Connection { return nil }

func (e *Event) Sender() *Sender { return nil }

func (c *Connection) NewSender(name string) *Sender { return nil }

func (c *Connection) Close() {}

type Container struct{}

type Sender struct{}

func (s *Sender) Credit() int { return 0 }

func (s *Sender) Send(m *Message) {}

func Run(connection net.Conn, handler interface{}) error { return nil }

type Message struct{ Body interface{} }
