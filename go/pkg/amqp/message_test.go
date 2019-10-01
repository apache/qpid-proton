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

	"github.com/apache/qpid-proton/go/pkg/internal/test"
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
	return test.Differ(m, m2)
}

func TestDefaultMessage(t *testing.T) {
	m := NewMessage()
	if err := roundTrip(m); err != nil {
		t.Error(err)
	}
	mv := reflect.ValueOf(m)
	// Check defaults
	for _, x := range []struct {
		method string
		want   interface{}
	}{
		{"Inferred", false},
		{"Durable", false},
		{"Priority", uint8(4)},
		{"TTL", time.Duration(0)},
		{"UserId", ""},
		{"Address", ""},
		{"Subject", ""},
		{"ReplyTo", ""},
		{"ContentType", ""},
		{"ContentEncoding", ""},
		{"GroupId", ""},
		{"GroupSequence", int32(0)},
		{"ReplyToGroupId", ""},
		{"MessageId", nil},
		{"CorrelationId", nil},
		{"DeliveryAnnotations", map[AnnotationKey]interface{}{}},
		{"MessageAnnotations", map[AnnotationKey]interface{}{}},
		{"ApplicationProperties", map[string]interface{}{}},

		// Deprecated
		{"Instructions", map[string]interface{}(nil)},
		{"Annotations", map[string]interface{}(nil)},
		{"Properties", map[string]interface{}{}},
		{"Body", nil},
	} {
		ret := mv.MethodByName(x.method).Call(nil)
		if err := test.Differ(x.want, ret[0].Interface()); err != nil {
			t.Errorf("%s: %s", x.method, err)
		}
	}
	if err := test.Differ("Message{}", m.String()); err != nil {
		t.Error(err)
	}
}

func TestMessageString(t *testing.T) {
	m := NewMessageWith("hello")
	m.SetInferred(false)
	m.SetUserId("user")
	m.SetDeliveryAnnotations(map[AnnotationKey]interface{}{AnnotationKeySymbol("instructions"): "foo"})
	m.SetMessageAnnotations(map[AnnotationKey]interface{}{AnnotationKeySymbol("annotations"): "bar"})
	m.SetApplicationProperties(map[string]interface{}{"int": int32(32)})
	if err := roundTrip(m); err != nil {
		t.Error(err)
	}
	msgstr := "Message{user-id: user, delivery-annotations: map[instructions:foo], message-annotations: map[annotations:bar], application-properties: map[int:32], body: hello}"
	if err := test.Differ(msgstr, m.String()); err != nil {
		t.Error(err)
	}
}

// Set all message properties
func setMessageProperties(m Message) Message {
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
	m.SetDeliveryAnnotations(map[AnnotationKey]interface{}{AnnotationKeySymbol("instructions"): "foo"})
	m.SetMessageAnnotations(map[AnnotationKey]interface{}{AnnotationKeySymbol("annotations"): "bar"})
	m.SetApplicationProperties(map[string]interface{}{"int": int32(32), "bool": true})
	return m
}

func TestMessageRoundTrip(t *testing.T) {
	m1 := NewMessage()
	setMessageProperties(m1)
	m1.Marshal("hello")

	buffer, err := m1.Encode(nil)
	if err != nil {
		t.Fatal(err)
	}
	m, err := DecodeMessage(buffer)
	if err != nil {
		t.Fatal(err)
	}
	if err = test.Differ(m1, m); err != nil {
		t.Error(err)
	}

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

		{m.DeliveryAnnotations(), map[AnnotationKey]interface{}{AnnotationKeySymbol("instructions"): "foo"}},
		{m.MessageAnnotations(), map[AnnotationKey]interface{}{AnnotationKeySymbol("annotations"): "bar"}},
		{m.ApplicationProperties(), map[string]interface{}{"int": int32(32), "bool": true}},
		{m.Body(), "hello"},

		// Deprecated
		{m.Instructions(), map[string]interface{}{"instructions": "foo"}},
		{m.Annotations(), map[string]interface{}{"annotations": "bar"}},
	} {
		if err := test.Differ(data[0], data[1]); err != nil {
			t.Error(err)
		}
	}
	if err := roundTrip(m); err != nil {
		t.Error(err)
	}
}

func TestDeprecated(t *testing.T) {
	m := NewMessage()

	m.SetInstructions(map[string]interface{}{"instructions": "foo"})
	m.SetAnnotations(map[string]interface{}{"annotations": "bar"})
	m.SetProperties(map[string]interface{}{"int": int32(32), "bool": true})

	for _, data := range [][]interface{}{
		{m.DeliveryAnnotations(), map[AnnotationKey]interface{}{AnnotationKeySymbol("instructions"): "foo"}},
		{m.MessageAnnotations(), map[AnnotationKey]interface{}{AnnotationKeySymbol("annotations"): "bar"}},
		{m.ApplicationProperties(), map[string]interface{}{"int": int32(32), "bool": true}},

		{m.Instructions(), map[string]interface{}{"instructions": "foo"}},
		{m.Annotations(), map[string]interface{}{"annotations": "bar"}},
		{m.Properties(), map[string]interface{}{"int": int32(32), "bool": true}},
	} {
		if err := test.Differ(data[0], data[1]); err != nil {
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
	if err := test.Differ(body.(int64), int64(42)); err != nil {
		t.Error(err)
	}
	if err := test.Differ(i, int64(42)); err != nil {
		t.Error(err)
	}

	m = NewMessageWith("hello")
	m.Unmarshal(&s)
	m.Unmarshal(&body)
	if err := test.Differ(s, "hello"); err != nil {
		t.Error(err)
	}
	if err := test.Differ(body.(string), "hello"); err != nil {
		t.Error(err)
	}
	if err := roundTrip(m); err != nil {
		t.Error(err)
	}

	m = NewMessageWith(Binary("bin"))
	m.Unmarshal(&s)
	m.Unmarshal(&body)
	if err := test.Differ(body.(Binary), Binary("bin")); err != nil {
		t.Error(err)
	}
	if err := test.Differ(s, "bin"); err != nil {
		t.Error(err)
	}
	if err := roundTrip(m); err != nil {
		t.Error(err)
	}

	// TODO aconway 2015-09-08: array etc.
}

// Benchmarks assign to package-scope variables to prevent being optimized out.
var bmM Message
var bmBuf []byte

func BenchmarkNewMessageEmpty(b *testing.B) {
	for n := 0; n < b.N; n++ {
		bmM = NewMessage()
	}
}

func BenchmarkNewMessageString(b *testing.B) {
	for n := 0; n < b.N; n++ {
		bmM = NewMessageWith("hello")
	}
}

func BenchmarkNewMessageAll(b *testing.B) {
	for n := 0; n < b.N; n++ {
		bmM = setMessageProperties(NewMessageWith("hello"))
	}
}

func BenchmarkEncode(b *testing.B) {
	m := setMessageProperties(NewMessageWith("hello"))
	b.ResetTimer()
	for n := 0; n < b.N; n++ {
		buf, err := m.Encode(nil)
		if err != nil {
			b.Fatal(err)
		}
		bmBuf = buf
	}
}

func BenchmarkDecode(b *testing.B) {
	var buf []byte
	buf, err := setMessageProperties(NewMessageWith("hello")).Encode(buf)
	if err != nil {
		b.Fatal(err)
	}
	m := NewMessage()
	b.ResetTimer()
	for n := 0; n < b.N; n++ {
		if err := m.Decode(buf); err != nil {
			b.Fatal(err)
		}
		bmM = m
	}
}
