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

package electron

import (
	"net"
	"qpid.apache.org/proton"
	"strconv"
	"sync/atomic"
)

// Container is an AMQP container, it represents a single AMQP "application".It
// provides functions to create new Connections to remote containers.
//
// Create with NewContainer()
//
type Container interface {
	// Id is a unique identifier for the container in your distributed application.
	Id() string

	// Create a new AMQP Connection over the supplied net.Conn connection.
	//
	// You must call Connection.Open() on the returned Connection, after
	// setting any Connection properties you need to set. Note the net.Conn
	// can be an outgoing connection (e.g. made with net.Dial) or an incoming
	// connection (e.g. made with net.Listener.Accept())
	Connection(net.Conn, ...ConnectionOption) (Connection, error)
}

type container struct {
	id         string
	tagCounter uint64
}

func (cont *container) nextTag() string {
	return strconv.FormatUint(atomic.AddUint64(&cont.tagCounter, 1), 32)
}

// NewContainer creates a new container. The id must be unique in your
// distributed application, all connections created by the container
// will have this container-id.
//
// If id == "" a random UUID will be generated for the id.
func NewContainer(id string) Container {
	if id == "" {
		id = proton.UUID4().String()
	}
	cont := &container{id: id}
	return cont
}

func (cont *container) Id() string { return cont.id }

func (cont *container) nextLinkName() string {
	return cont.id + "@" + cont.nextTag()
}

func (cont *container) Connection(conn net.Conn, setting ...ConnectionOption) (Connection, error) {
	return newConnection(conn, cont, setting...)
}
