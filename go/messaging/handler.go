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

package messaging

import (
	"qpid.apache.org/proton/go/amqp"
	"qpid.apache.org/proton/go/event"
)

// FIXME aconway 2015-04-28: cleanup - exposing delivery vs. disposition.

type acksMap map[event.Delivery]chan Disposition
type receiverMap map[event.Link]chan amqp.Message

type handler struct {
	connection *Connection
	acks       acksMap
	receivers  receiverMap
}

func newHandler(c *Connection) *handler {
	return &handler{c, make(acksMap), make(receiverMap)}
}

func (h *handler) HandleMessagingEvent(t event.MessagingEventType, e event.Event) error {
	switch t {
	// FIXME aconway 2015-04-29: handle errors.
	case event.MConnectionClosed:
		for _, ack := range h.acks {
			// FIXME aconway 2015-04-29: communicate error info
			close(ack)
		}

	case event.MSettled:
		ack := h.acks[e.Delivery()]
		if ack != nil {
			ack <- Disposition(e.Delivery().Remote().Type())
			close(ack)
			delete(h.acks, e.Delivery())
		}

	case event.MMessage:
		r := h.receivers[e.Link()]
		if r != nil {
			m, _ := event.DecodeMessage(e)
			// FIXME aconway 2015-04-29: hack, direct send, possible blocking.
			r <- m
		} else {
			// FIXME aconway 2015-04-29: Message with no receiver - log? panic? deadletter? drop?
		}
	}
	return nil
}
