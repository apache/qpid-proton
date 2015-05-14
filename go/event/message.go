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

package event

// #include <proton/types.h>
// #include <proton/message.h>
// #include <proton/codec.h>
import "C"

import (
	"qpid.apache.org/proton/go/amqp"
	"qpid.apache.org/proton/go/internal"
)

// DecodeMessage decodes the message containined in a delivery event.
func DecodeMessage(e Event) (m amqp.Message, err error) {
	defer internal.DoRecover(&err)
	delivery := e.Delivery()
	if !delivery.Readable() || delivery.Partial() {
		return nil, internal.Errorf("attempting to get incomplete message")
	}
	data := make([]byte, delivery.Pending())
	result := delivery.Link().Recv(data)
	if result != len(data) {
		return nil, internal.Errorf("cannot receive message: %s", internal.PnErrorCode(result))
	}
	return amqp.DecodeMessage(data)
}

// FIXME aconway 2015-04-08: proper handling of delivery tags. Tag counter per link.
var tags amqp.UidCounter

// Send sends a amqp.Message over a Link.
// Returns a Delivery that can be use to determine the outcome of the message.
func (link Link) Send(m amqp.Message) (Delivery, error) {
	if !link.IsSender() {
		return Delivery{}, internal.Errorf("attempt to send message on receiving link")
	}
	// FIXME aconway 2015-04-08: buffering, error handling
	delivery := link.Delivery(tags.Next())
	bytes, err := m.Encode(nil)
	if err != nil {
		return Delivery{}, internal.Errorf("cannot send mesage %s", err)
	}
	result := link.SendBytes(bytes)
	link.Advance()
	if result != len(bytes) {
		if result < 0 {
			return delivery, internal.Errorf("send failed %v", internal.PnErrorCode(result))
		} else {
			return delivery, internal.Errorf("send incomplete %v of %v", result, len(bytes))
		}
	}
	if link.RemoteSndSettleMode() == PnSndSettled { // FIXME aconway 2015-04-08: enum names
		delivery.Settle()
	}
	return delivery, nil
}
