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
	"testing"
	"time"
)

func roundTrip(m Message) error {
	buffer, err := m.Encode(nil)
	if err != nil {
		return err
	}
	m2, err := DecodeMessage(buffer)
	if err != nil {
		return err
	}
	return checkEqual(m, m2)
}

func TestDefaultMessage(t *testing.T) {
	m := NewMessage()
	// Check defaults
	for _, data := range [][]interface{}{
		{m.Inferred(), false},
		{m.Durable(), false},
		{m.Priority(), uint8(4)},
		{m.TTL(), time.Duration(0)},
		{m.UserId(), ""},
		{m.Address(), ""},
		{m.Subject(), ""},
		{m.ReplyTo(), ""},
		{m.ContentType(), ""},
		{m.ContentEncoding(), ""},
		{m.GroupId(), ""},
		{m.GroupSequence(), int32(0)},
		{m.ReplyToGroupId(), ""},
		{m.MessageId(), nil},
		{m.CorrelationId(), nil},
		{m.Instructions(), map[string]interface{}{}},
		{m.Annotations(), map[string]interface{}{}},
		{m.Properties(), map[string]interface{}{}},
		{m.Body(), nil},
	} {
		if err := checkEqual(data[0], data[1]); err != nil {
			t.Error(err)
		}
	}
	if err := roundTrip(m); err != nil {
		t.Error(err)
	}
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
	m.SetInstructions(map[string]interface{}{"instructions": "foo"})
	m.SetAnnotations(map[string]interface{}{"annotations": "foo"})
	m.SetProperties(map[string]interface{}{"int": int32(32), "bool": true, "string": "foo"})
	m.Marshal("hello")

	for _, data := range [][]interface{}{
		{m.Inferred(), false},
		{m.Durable(), true},
		{m.Priority(), uint8(42)},
		{m.TTL(), time.Duration(0)},
		{m.UserId(), "user"},
		{m.Address(), "address"},
		{m.Subject(), "subject"},
		{m.ReplyTo(), "replyto"},
		{m.ContentType(), "content"},
		{m.ContentEncoding(), "encoding"},
		{m.GroupId(), "group"},
		{m.GroupSequence(), int32(42)},
		{m.ReplyToGroupId(), "replytogroup"},
		{m.MessageId(), "id"},
		{m.CorrelationId(), "correlation"},
		{m.Instructions(), map[string]interface{}{"instructions": "foo"}},
		{m.Annotations(), map[string]interface{}{"annotations": "foo"}},
		{m.Properties(), map[string]interface{}{"int": int32(32), "bool": true, "string": "foo"}},
		{m.Body(), "hello"},
	} {
		if err := checkEqual(data[0], data[1]); err != nil {
			t.Error(err)
		}
	}
	if err := roundTrip(m); err != nil {
		t.Error(err)
	}
}

func TestMessageBodyTypes(t *testing.T) {
	var s string
	var body interface{}
	var i int64

	m := NewMessageWith(int64(42))
	m.Unmarshal(&body)
	m.Unmarshal(&i)
	if err := checkEqual(body.(int64), int64(42)); err != nil {
		t.Error(err)
	}
	if err := checkEqual(i, int64(42)); err != nil {
		t.Error(err)
	}

	m = NewMessageWith("hello")
	m.Unmarshal(&s)
	m.Unmarshal(&body)
	if err := checkEqual(s, "hello"); err != nil {
		t.Error(err)
	}
	if err := checkEqual(body.(string), "hello"); err != nil {
		t.Error(err)
	}
	if err := roundTrip(m); err != nil {
		t.Error(err)
	}

	m = NewMessageWith(Binary("bin"))
	m.Unmarshal(&s)
	m.Unmarshal(&body)
	if err := checkEqual(body.(Binary), Binary("bin")); err != nil {
		t.Error(err)
	}
	if err := checkEqual(s, "bin"); err != nil {
		t.Error(err)
	}
	if err := roundTrip(m); err != nil {
		t.Error(err)
	}

	// TODO aconway 2015-09-08: array etc.
}
