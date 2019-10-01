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

// #include <proton/types.h>
// #include <proton/message.h>
// #include <proton/codec.h>
import "C"

import (
	"fmt"
	"strconv"
	"sync/atomic"

	"github.com/apache/qpid-proton/go/pkg/amqp"
)

// HasMessage is true if all message data is available.
// Equivalent to !d.isNil && d.Readable() && !d.Partial()
func (d Delivery) HasMessage() bool { return !d.IsNil() && d.Readable() && !d.Partial() }

// Message decodes the message contained in a delivery.
//
// Must be called in the correct link context with this delivery as the current message,
// handling an MMessage event is always a safe context to call this function.
//
// Will return an error if message is incomplete or not current.
func (delivery Delivery) Message() (amqp.Message, error) {
	var err error
	bytes, err := delivery.MessageBytes()
	if err == nil {
		m := amqp.NewMessage()
		err = m.Decode(bytes)
		return m, err
	}
	return nil, err
}

// MessageBytes extracts the raw message bytes contained in a delivery.
//
// Must be called in the correct link context with this delivery as the current message,
// handling an MMessage event is always a safe context to call this function.
//
// Will return an error if message is incomplete or not current.
func (delivery Delivery) MessageBytes() ([]byte, error) {
	if !delivery.Readable() {
		return nil, fmt.Errorf("delivery is not readable")
	}
	if delivery.Partial() {
		return nil, fmt.Errorf("delivery has partial message")
	}
	data := make([]byte, delivery.Pending())
	result := delivery.Link().Recv(data)
	if result != len(data) {
		return nil, fmt.Errorf("cannot receive message: %s", PnErrorCode(result))
	}
	return data, nil
}

// Process-wide atomic counter for generating tag names
var tagCounter uint64

func nextTag() string {
	return strconv.FormatUint(atomic.AddUint64(&tagCounter, 1), 32)
}

// Send sends a amqp.Message over a Link.
// Returns a Delivery that can be use to determine the outcome of the message.
func (link Link) Send(m amqp.Message) (Delivery, error) {
	bytes, err := m.Encode(nil)
	if err != nil {
		return Delivery{}, err
	}
	d, err := link.SendMessageBytes(bytes)
	return d, err
}

// SendMessageBytes sends encoded bytes of an amqp.Message over a Link.
// Returns a Delivery that can be use to determine the outcome of the message.
func (link Link) SendMessageBytes(bytes []byte) (Delivery, error) {
	if !link.IsSender() {
		return Delivery{}, fmt.Errorf("attempt to send message on receiving link")
	}
	delivery := link.Delivery(nextTag())
	result := link.SendBytes(bytes)
	link.Advance()
	if result != len(bytes) {
		if result < 0 {
			return delivery, fmt.Errorf("send failed %v", PnErrorCode(result))
		} else {
			return delivery, fmt.Errorf("send incomplete %v of %v", result, len(bytes))
		}
	}
	if link.RemoteSndSettleMode() == SndSettled {
		delivery.Settle()
	}
	return delivery, nil
}
