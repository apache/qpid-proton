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
	"reflect"
	"testing"
	"time"
)

func roundTrip(t *testing.T, m Message) {
	buffer, err := m.Encode(nil)
	if err != nil {
		t.Fatalf("Encode failed: %v", err)
	}
	m2, err := DecodeMessage(buffer)
	if err != nil {
		t.Fatalf("Decode failed: %v", err)
	}
	if !reflect.DeepEqual(m, m2) {
		t.Errorf("Message mismatch got\n%#v\nwant\n%#v", m, m2)
	}
}

func TestDefaultMessageRoundTrip(t *testing.T) {
	m := NewMessage()
	// Check defaults
	assertEqual(m.Inferred(), false)
	assertEqual(m.Durable(), false)
	assertEqual(m.Priority(), uint8(4))
	assertEqual(m.TTL(), time.Duration(0))
	assertEqual(m.UserId(), "")
	assertEqual(m.Address(), "")
	assertEqual(m.Subject(), "")
	assertEqual(m.ReplyTo(), "")
	assertEqual(m.ContentType(), "")
	assertEqual(m.ContentEncoding(), "")
	assertEqual(m.GroupId(), "")
	assertEqual(m.GroupSequence(), int32(0))
	assertEqual(m.ReplyToGroupId(), "")
	assertEqual(m.MessageId(), nil)
	assertEqual(m.CorrelationId(), nil)
	assertEqual(*m.Instructions(), map[string]interface{}{})
	assertEqual(*m.Annotations(), map[string]interface{}{})
	assertEqual(*m.Properties(), map[string]interface{}{})
	assertEqual(m.Body(), nil)

	roundTrip(t, m)
}

func TestMessageRoundTrip(t *testing.T) {
	m := NewMessage()
	m.SetInferred(false)
	m.SetDurable(true)
	m.SetPriority(42)
	m.SetTTL(0)
	m.SetUserId("user")
	m.SetAddress("address")
	m.SetSubject("subject")
	m.SetReplyTo("replyto")
	m.SetContentType("content")
	m.SetContentEncoding("encoding")
	m.SetGroupId("group")
	m.SetGroupSequence(42)
	m.SetReplyToGroupId("replytogroup")
	m.SetMessageId("id")
	m.SetCorrelationId("correlation")
	*m.Instructions() = map[string]interface{}{"instructions": "foo"}
	*m.Annotations() = map[string]interface{}{"annotations": "foo"}
	*m.Properties() = map[string]interface{}{"int": int32(32), "bool": true, "string": "foo"}
	m.SetBody("hello")
	roundTrip(t, m)
}
