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
	"strconv"
	"sync/atomic"

	"github.com/apache/qpid-proton/go/pkg/proton"
)

// Container is an AMQP container, it represents a single AMQP "application"
// which can have multiple client or server connections.
//
// Each Container in a distributed AMQP application must have a unique
// container-id which is applied to its connections.
//
// Create with NewContainer()
//
type Container interface {
	// Id is a unique identifier for the container in your distributed application.
	Id() string

	// Connection creates a connection associated with this container.
	Connection(conn net.Conn, opts ...ConnectionOption) (Connection, error)

	// Dial is shorthand for
	//     conn, err := net.Dial(); c, err := Connection(conn, opts...)
	// See net.Dial() for the meaning of the network, address arguments.
	Dial(network string, address string, opts ...ConnectionOption) (Connection, error)

	// Accept is shorthand for:
	//     conn, err := l.Accept(); c, err := Connection(conn, append(opts, Server()...)
	Accept(l net.Listener, opts ...ConnectionOption) (Connection, error)

	// String returns Id()
	String() string
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

func (cont *container) String() string { return cont.Id() }

func (cont *container) nextLinkName() string {
	return cont.id + "@" + cont.nextTag()
}

func (cont *container) Connection(conn net.Conn, opts ...ConnectionOption) (Connection, error) {
	return NewConnection(conn, append(opts, Parent(cont))...)
}

func (cont *container) Dial(network, address string, opts ...ConnectionOption) (c Connection, err error) {
	conn, err := net.Dial(network, address)
	if err == nil {
		c, err = cont.Connection(conn, opts...)
	}
	return
}

func (cont *container) Accept(l net.Listener, opts ...ConnectionOption) (c Connection, err error) {
	conn, err := l.Accept()
	if err == nil {
		c, err = cont.Connection(conn, append([]ConnectionOption{Server()}, opts...)...)
	}
	return
}
