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

// #include <proton/codec.h>
// #include <proton/types.h>
// #include <proton/message.h>
// #include <stdlib.h>
//
// /* Helper for setting message string fields */
// typedef int (*set_fn)(pn_message_t*, const char*);
// int msg_set_str(pn_message_t* m, char* s, set_fn set) {
//     int result = set(m, s);
//     free(s);
//     return result;
// }
//
import "C"

import (
	"fmt"
	"runtime"
	"time"
)

// Message is the interface to an AMQP message.
type Message interface {
	// Durable indicates that any parties taking responsibility
	// for the message must durably store the content.
	Durable() bool
	SetDurable(bool)

	// Priority impacts ordering guarantees. Within a
	// given ordered context, higher priority messages may jump ahead of
	// lower priority messages.
	Priority() uint8
	SetPriority(uint8)

	// TTL or Time To Live, a message it may be dropped after this duration
	TTL() time.Duration
	SetTTL(time.Duration)

	// FirstAcquirer indicates
	// that the recipient of the message is the first recipient to acquire
	// the message, i.e. there have been no failed delivery attempts to
	// other acquirers. Note that this does not mean the message has not
	// been delivered to, but not acquired, by other recipients.
	FirstAcquirer() bool
	SetFirstAcquirer(bool)

	// DeliveryCount tracks how many attempts have been made to
	// delivery a message.
	DeliveryCount() uint32
	SetDeliveryCount(uint32)

	// MessageId provides a unique identifier for a message.
	// it can be an a string, an unsigned long, a uuid or a
	// binary value.
	MessageId() interface{}
	SetMessageId(interface{})

	UserId() string
	SetUserId(string)

	Address() string
	SetAddress(string)

	Subject() string
	SetSubject(string)

	ReplyTo() string
	SetReplyTo(string)

	// CorrelationId is set on correlated request and response messages. It can be
	// an a string, an unsigned long, a uuid or a binary value.
	CorrelationId() interface{}
	SetCorrelationId(interface{})

	ContentType() string
	SetContentType(string)

	ContentEncoding() string
	SetContentEncoding(string)

	// ExpiryTime indicates an absolute time when the message may be dropped.
	// A Zero time (i.e. t.isZero() == true) indicates a message never expires.
	ExpiryTime() time.Time
	SetExpiryTime(time.Time)

	CreationTime() time.Time
	SetCreationTime(time.Time)

	GroupId() string
	SetGroupId(string)

	GroupSequence() int32
	SetGroupSequence(int32)

	ReplyToGroupId() string
	SetReplyToGroupId(string)

	// Property map set by the application to be carried with the message.
	// Values must be simple types (not maps, lists or sequences)
	ApplicationProperties() map[string]interface{}
	SetApplicationProperties(map[string]interface{})

	// Per-delivery annotations to provide delivery instructions.
	// May be added or removed by intermediaries during delivery.
	DeliveryAnnotations() map[AnnotationKey]interface{}
	SetDeliveryAnnotations(map[AnnotationKey]interface{})

	// Message annotations added as part of the bare message at creation, usually
	// by an AMQP library. See ApplicationProperties() for adding application data.
	MessageAnnotations() map[AnnotationKey]interface{}
	SetMessageAnnotations(map[AnnotationKey]interface{})

	// Inferred indicates how the message content
	// is encoded into AMQP sections. If inferred is true then binary and
	// list values in the body of the message will be encoded as AMQP DATA
	// and AMQP SEQUENCE sections, respectively. If inferred is false,
	// then all values in the body of the message will be encoded as AMQP
	// VALUE sections regardless of their type.
	Inferred() bool
	SetInferred(bool)

	// Marshal a Go value into the message body. See amqp.Marshal() for details.
	Marshal(interface{})

	// Unmarshal the message body into the value pointed to by v. See amqp.Unmarshal() for details.
	Unmarshal(interface{})

	// Body value resulting from the default unmarshaling of message body as interface{}
	Body() interface{}

	// Encode encodes the message as AMQP data. If buffer is non-nil and is large enough
	// the message is encoded into it, otherwise a new buffer is created.
	// Returns the buffer containing the message.
	Encode(buffer []byte) ([]byte, error)

	// Decode data into this message. Overwrites an existing message content.
	Decode(buffer []byte) error

	// Clear the message contents.
	Clear()

	// Copy the contents of another message to this one.
	Copy(m Message) error

	// Deprecated: use DeliveryAnnotations() for a more type-safe interface
	Instructions() map[string]interface{}
	SetInstructions(v map[string]interface{})

	// Deprecated: use MessageAnnotations() for a more type-safe interface
	Annotations() map[string]interface{}
	SetAnnotations(v map[string]interface{})

	// Deprecated: use ApplicationProperties() for a more type-safe interface
	Properties() map[string]interface{}
	SetProperties(v map[string]interface{})
}

type message struct{ pn *C.pn_message_t }

func freeMessage(m *message) {
	C.pn_message_free(m.pn)
	m.pn = nil
}

// NewMessage creates a new message instance.
func NewMessage() Message {
	m := &message{C.pn_message()}
	runtime.SetFinalizer(m, freeMessage)
	return m
}

// NewMessageWith creates a message with value as the body. Equivalent to
//     m := NewMessage(); m.Marshal(body)
func NewMessageWith(value interface{}) Message {
	m := NewMessage()
	m.Marshal(value)
	return m
}

func (m *message) Clear() { C.pn_message_clear(m.pn) }

func (m *message) Copy(x Message) error {
	if data, err := x.Encode(nil); err == nil {
		return m.Decode(data)
	} else {
		return err
	}
}

// ==== message get functions

func rewindGet(data *C.pn_data_t) (v interface{}) {
	C.pn_data_rewind(data)
	C.pn_data_next(data)
	unmarshal(&v, data)
	return v
}

func (m *message) Inferred() bool  { return bool(C.pn_message_is_inferred(m.pn)) }
func (m *message) Durable() bool   { return bool(C.pn_message_is_durable(m.pn)) }
func (m *message) Priority() uint8 { return uint8(C.pn_message_get_priority(m.pn)) }
func (m *message) TTL() time.Duration {
	return time.Duration(C.pn_message_get_ttl(m.pn)) * time.Millisecond
}
func (m *message) FirstAcquirer() bool        { return bool(C.pn_message_is_first_acquirer(m.pn)) }
func (m *message) DeliveryCount() uint32      { return uint32(C.pn_message_get_delivery_count(m.pn)) }
func (m *message) MessageId() interface{}     { return rewindGet(C.pn_message_id(m.pn)) }
func (m *message) UserId() string             { return goString(C.pn_message_get_user_id(m.pn)) }
func (m *message) Address() string            { return C.GoString(C.pn_message_get_address(m.pn)) }
func (m *message) Subject() string            { return C.GoString(C.pn_message_get_subject(m.pn)) }
func (m *message) ReplyTo() string            { return C.GoString(C.pn_message_get_reply_to(m.pn)) }
func (m *message) CorrelationId() interface{} { return rewindGet(C.pn_message_correlation_id(m.pn)) }
func (m *message) ContentType() string        { return C.GoString(C.pn_message_get_content_type(m.pn)) }
func (m *message) ContentEncoding() string    { return C.GoString(C.pn_message_get_content_encoding(m.pn)) }

func (m *message) ExpiryTime() time.Time {
	return time.Unix(0, int64(time.Millisecond*time.Duration(C.pn_message_get_expiry_time(m.pn))))
}
func (m *message) CreationTime() time.Time {
	return time.Unix(0, int64(time.Millisecond)*int64(C.pn_message_get_creation_time(m.pn)))
}
func (m *message) GroupId() string        { return C.GoString(C.pn_message_get_group_id(m.pn)) }
func (m *message) GroupSequence() int32   { return int32(C.pn_message_get_group_sequence(m.pn)) }
func (m *message) ReplyToGroupId() string { return C.GoString(C.pn_message_get_reply_to_group_id(m.pn)) }

func getAnnotations(data *C.pn_data_t) (v map[AnnotationKey]interface{}) {
	if C.pn_data_size(data) > 0 {
		C.pn_data_rewind(data)
		C.pn_data_next(data)
		unmarshal(&v, data)
	}
	return v
}

func (m *message) DeliveryAnnotations() map[AnnotationKey]interface{} {
	return getAnnotations(C.pn_message_instructions(m.pn))
}
func (m *message) MessageAnnotations() map[AnnotationKey]interface{} {
	return getAnnotations(C.pn_message_annotations(m.pn))
}

func (m *message) ApplicationProperties() map[string]interface{} {
	var v map[string]interface{}
	data := C.pn_message_properties(m.pn)
	if C.pn_data_size(data) > 0 {
		C.pn_data_rewind(data)
		C.pn_data_next(data)
		unmarshal(&v, data)
	}
	return v
}

// ==== message set methods

func setData(v interface{}, data *C.pn_data_t) {
	C.pn_data_clear(data)
	marshal(v, data)
}

func (m *message) SetInferred(b bool)  { C.pn_message_set_inferred(m.pn, C.bool(b)) }
func (m *message) SetDurable(b bool)   { C.pn_message_set_durable(m.pn, C.bool(b)) }
func (m *message) SetPriority(b uint8) { C.pn_message_set_priority(m.pn, C.uint8_t(b)) }
func (m *message) SetTTL(d time.Duration) {
	C.pn_message_set_ttl(m.pn, C.pn_millis_t(d/time.Millisecond))
}
func (m *message) SetFirstAcquirer(b bool)     { C.pn_message_set_first_acquirer(m.pn, C.bool(b)) }
func (m *message) SetDeliveryCount(c uint32)   { C.pn_message_set_delivery_count(m.pn, C.uint32_t(c)) }
func (m *message) SetMessageId(id interface{}) { setData(id, C.pn_message_id(m.pn)) }
func (m *message) SetUserId(s string)          { C.pn_message_set_user_id(m.pn, pnBytes(([]byte)(s))) }
func (m *message) SetAddress(s string) {
	C.msg_set_str(m.pn, C.CString(s), C.set_fn(C.pn_message_set_address))
}
func (m *message) SetSubject(s string) {
	C.msg_set_str(m.pn, C.CString(s), C.set_fn(C.pn_message_set_subject))
}
func (m *message) SetReplyTo(s string) {
	C.msg_set_str(m.pn, C.CString(s), C.set_fn(C.pn_message_set_reply_to))
}
func (m *message) SetCorrelationId(c interface{}) { setData(c, C.pn_message_correlation_id(m.pn)) }
func (m *message) SetContentType(s string) {
	C.msg_set_str(m.pn, C.CString(s), C.set_fn(C.pn_message_set_content_type))
}
func (m *message) SetContentEncoding(s string) {
	C.msg_set_str(m.pn, C.CString(s), C.set_fn(C.pn_message_set_content_encoding))
}
func (m *message) SetExpiryTime(t time.Time)   { C.pn_message_set_expiry_time(m.pn, pnTime(t)) }
func (m *message) SetCreationTime(t time.Time) { C.pn_message_set_creation_time(m.pn, pnTime(t)) }
func (m *message) SetGroupId(s string) {
	C.msg_set_str(m.pn, C.CString(s), C.set_fn(C.pn_message_set_group_id))
}
func (m *message) SetGroupSequence(s int32) {
	C.pn_message_set_group_sequence(m.pn, C.pn_sequence_t(s))
}
func (m *message) SetReplyToGroupId(s string) {
	C.msg_set_str(m.pn, C.CString(s), C.set_fn(C.pn_message_set_reply_to_group_id))
}

func (m *message) SetDeliveryAnnotations(v map[AnnotationKey]interface{}) {
	setData(v, C.pn_message_instructions(m.pn))
}
func (m *message) SetMessageAnnotations(v map[AnnotationKey]interface{}) {
	setData(v, C.pn_message_annotations(m.pn))
}
func (m *message) SetApplicationProperties(v map[string]interface{}) {
	setData(v, C.pn_message_properties(m.pn))
}

// Marshal body from v
func (m *message) Marshal(v interface{}) { clearMarshal(v, C.pn_message_body(m.pn)) }

// Unmarshal body to v, which must be a pointer to a value. See amqp.Unmarshal
func (m *message) Unmarshal(v interface{}) {
	data := C.pn_message_body(m.pn)
	if C.pn_data_size(data) > 0 {
		C.pn_data_rewind(data)
		C.pn_data_next(data)
		unmarshal(v, data)
	}
	return
}

// Return the body value as an interface
func (m *message) Body() (v interface{}) { m.Unmarshal(&v); return }

func (m *message) Decode(data []byte) error {
	m.Clear()
	if len(data) == 0 {
		return fmt.Errorf("empty buffer for decode")
	}
	if C.pn_message_decode(m.pn, cPtr(data), cLen(data)) < 0 {
		return fmt.Errorf("decoding message: %s", PnError(C.pn_message_error(m.pn)))
	}
	return nil
}

func DecodeMessage(data []byte) (m Message, err error) {
	m = NewMessage()
	err = m.Decode(data)
	return
}

func (m *message) Encode(buffer []byte) ([]byte, error) {
	encode := func(buf []byte) ([]byte, error) {
		len := cLen(buf)
		result := C.pn_message_encode(m.pn, cPtr(buf), &len)
		switch {
		case result == C.PN_OVERFLOW:
			return buf, overflow
		case result < 0:
			return buf, fmt.Errorf("cannot encode message: %s", PnErrorCode(result))
		default:
			return buf[:len], nil
		}
	}
	return encodeGrow(buffer, encode)
}

// TODO aconway 2015-09-14: Multi-section messages.

// TODO aconway 2016-09-09: Message.String() use inspect.

// ==== Deprecated functions
func oldGetAnnotations(data *C.pn_data_t) (v map[string]interface{}) {
	if C.pn_data_size(data) > 0 {
		C.pn_data_rewind(data)
		C.pn_data_next(data)
		unmarshal(&v, data)
	}
	return v
}

func (m *message) Instructions() map[string]interface{} {
	return oldGetAnnotations(C.pn_message_instructions(m.pn))
}
func (m *message) Annotations() map[string]interface{} {
	return oldGetAnnotations(C.pn_message_annotations(m.pn))
}
func (m *message) Properties() map[string]interface{} {
	return oldGetAnnotations(C.pn_message_properties(m.pn))
}

// Convert old string-keyed annotations to an AnnotationKey map
func fixAnnotations(old map[string]interface{}) (annotations map[AnnotationKey]interface{}) {
	annotations = make(map[AnnotationKey]interface{})
	for k, v := range old {
		annotations[AnnotationKeyString(k)] = v
	}
	return
}

func (m *message) SetInstructions(v map[string]interface{}) {
	setData(fixAnnotations(v), C.pn_message_instructions(m.pn))
}
func (m *message) SetAnnotations(v map[string]interface{}) {
	setData(fixAnnotations(v), C.pn_message_annotations(m.pn))
}
func (m *message) SetProperties(v map[string]interface{}) {
	setData(fixAnnotations(v), C.pn_message_properties(m.pn))
}
