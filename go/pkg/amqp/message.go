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
	"bytes"
	"fmt"
	"reflect"
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

	// Properties set by the application to be carried with the message.
	// Values must be simple types (not maps, lists or sequences)
	ApplicationProperties() map[string]interface{}
	SetApplicationProperties(map[string]interface{})

	// Per-delivery annotations to provide delivery instructions.
	// May be added or removed by intermediaries during delivery.
	// See ApplicationProperties() for properties set by the application.
	DeliveryAnnotations() map[AnnotationKey]interface{}
	SetDeliveryAnnotations(map[AnnotationKey]interface{})

	// Message annotations added as part of the bare message at creation, usually
	// by an AMQP library. See ApplicationProperties() for properties set by the application.
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

	// Get the message body, using the amqp.Unmarshal() rules for interface{}
	Body() interface{}

	// Set the body using amqp.Marshal()
	SetBody(interface{})

	// Marshal a Go value into the message body, synonym for SetBody()
	Marshal(interface{})

	// Unmarshal the message body, using amqp.Unmarshal()
	Unmarshal(interface{})

	// Encode encodes the message as AMQP data. If buffer is non-nil and is large enough
	// the message is encoded into it, otherwise a new buffer is created.
	// Returns the buffer containing the message.
	Encode(buffer []byte) ([]byte, error)

	// Decode data into this message. Overwrites an existing message content.
	Decode(buffer []byte) error

	// Clear the message contents, set all fields to the default value.
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

	// Human-readable string showing message contents and properties
	String() string
}

// NewMessage creates a new message instance.
func NewMessage() Message {
	m := &message{}
	m.Clear()
	return m
}

// NewMessageWith creates a message with value as the body.
func NewMessageWith(value interface{}) Message {
	m := NewMessage()
	m.SetBody(value)
	return m
}

// NewMessageCopy creates a copy of an existing message.
func NewMessageCopy(m Message) Message {
	m2 := NewMessage()
	m2.Copy(m)
	return m2
}

// Reset message to all default values
func (m *message) Clear() { *m = message{priority: 4} }

// Copy makes a deep copy of message x
func (m *message) Copy(x Message) error {
	var mc MessageCodec
	bytes, err := mc.Encode(x, nil)
	if err == nil {
		err = mc.Decode(m, bytes)
	}
	return err
}

type message struct {
	address               string
	applicationProperties map[string]interface{}
	contentEncoding       string
	contentType           string
	correlationId         interface{}
	creationTime          time.Time
	deliveryAnnotations   map[AnnotationKey]interface{}
	deliveryCount         uint32
	durable               bool
	expiryTime            time.Time
	firstAcquirer         bool
	groupId               string
	groupSequence         int32
	inferred              bool
	messageAnnotations    map[AnnotationKey]interface{}
	messageId             interface{}
	priority              uint8
	replyTo               string
	replyToGroupId        string
	subject               string
	ttl                   time.Duration
	userId                string
	body                  interface{}
	// Keep the original data to support Unmarshal to a non-interface{} type
	// Waste of memory, consider deprecating or making it optional.
	pnBody *C.pn_data_t
}

// ==== message get methods
func (m *message) Body() interface{}          { return m.body }
func (m *message) Inferred() bool             { return m.inferred }
func (m *message) Durable() bool              { return m.durable }
func (m *message) Priority() uint8            { return m.priority }
func (m *message) TTL() time.Duration         { return m.ttl }
func (m *message) FirstAcquirer() bool        { return m.firstAcquirer }
func (m *message) DeliveryCount() uint32      { return m.deliveryCount }
func (m *message) MessageId() interface{}     { return m.messageId }
func (m *message) UserId() string             { return m.userId }
func (m *message) Address() string            { return m.address }
func (m *message) Subject() string            { return m.subject }
func (m *message) ReplyTo() string            { return m.replyTo }
func (m *message) CorrelationId() interface{} { return m.correlationId }
func (m *message) ContentType() string        { return m.contentType }
func (m *message) ContentEncoding() string    { return m.contentEncoding }
func (m *message) ExpiryTime() time.Time      { return m.expiryTime }
func (m *message) CreationTime() time.Time    { return m.creationTime }
func (m *message) GroupId() string            { return m.groupId }
func (m *message) GroupSequence() int32       { return m.groupSequence }
func (m *message) ReplyToGroupId() string     { return m.replyToGroupId }

func (m *message) DeliveryAnnotations() map[AnnotationKey]interface{} {
	if m.deliveryAnnotations == nil {
		m.deliveryAnnotations = make(map[AnnotationKey]interface{})
	}
	return m.deliveryAnnotations
}
func (m *message) MessageAnnotations() map[AnnotationKey]interface{} {
	if m.messageAnnotations == nil {
		m.messageAnnotations = make(map[AnnotationKey]interface{})
	}
	return m.messageAnnotations
}
func (m *message) ApplicationProperties() map[string]interface{} {
	if m.applicationProperties == nil {
		m.applicationProperties = make(map[string]interface{})
	}
	return m.applicationProperties
}

// ==== message set methods

func (m *message) SetBody(v interface{})          { m.body = v }
func (m *message) SetInferred(x bool)             { m.inferred = x }
func (m *message) SetDurable(x bool)              { m.durable = x }
func (m *message) SetPriority(x uint8)            { m.priority = x }
func (m *message) SetTTL(x time.Duration)         { m.ttl = x }
func (m *message) SetFirstAcquirer(x bool)        { m.firstAcquirer = x }
func (m *message) SetDeliveryCount(x uint32)      { m.deliveryCount = x }
func (m *message) SetMessageId(x interface{})     { m.messageId = x }
func (m *message) SetUserId(x string)             { m.userId = x }
func (m *message) SetAddress(x string)            { m.address = x }
func (m *message) SetSubject(x string)            { m.subject = x }
func (m *message) SetReplyTo(x string)            { m.replyTo = x }
func (m *message) SetCorrelationId(x interface{}) { m.correlationId = x }
func (m *message) SetContentType(x string)        { m.contentType = x }
func (m *message) SetContentEncoding(x string)    { m.contentEncoding = x }
func (m *message) SetExpiryTime(x time.Time)      { m.expiryTime = x }
func (m *message) SetCreationTime(x time.Time)    { m.creationTime = x }
func (m *message) SetGroupId(x string)            { m.groupId = x }
func (m *message) SetGroupSequence(x int32)       { m.groupSequence = x }
func (m *message) SetReplyToGroupId(x string)     { m.replyToGroupId = x }

func (m *message) SetDeliveryAnnotations(x map[AnnotationKey]interface{}) {
	m.deliveryAnnotations = x
}
func (m *message) SetMessageAnnotations(x map[AnnotationKey]interface{}) {
	m.messageAnnotations = x
}
func (m *message) SetApplicationProperties(x map[string]interface{}) {
	m.applicationProperties = x
}

// Marshal body from v, same as SetBody(v). See amqp.Marshal.
func (m *message) Marshal(v interface{}) { m.body = v }

func (m *message) Unmarshal(v interface{}) {
	pnData := C.pn_data(2)
	defer C.pn_data_free(pnData)
	marshal(m.body, pnData)
	unmarshal(v, pnData)
}

// Internal use only
type MessageCodec struct {
	pn *C.pn_message_t // Cache a pn_message_t to speed up encode/decode
	// Optionally remember a byte buffer to use with MessageCodec methods.
	Buffer []byte
}

func (mc *MessageCodec) pnMessage() *C.pn_message_t {
	if mc.pn == nil {
		mc.pn = C.pn_message()
	}
	return mc.pn
}

func (mc *MessageCodec) Close() {
	if mc.pn != nil {
		C.pn_message_free(mc.pn)
		mc.pn = nil
	}
}

func (mc *MessageCodec) Decode(m Message, data []byte) error {
	pn := mc.pnMessage()
	if C.pn_message_decode(pn, cPtr(data), cLen(data)) < 0 {
		return fmt.Errorf("decoding message: %s", PnError(C.pn_message_error(pn)))
	}
	m.(*message).get(pn)
	return nil
}

func (m *message) Decode(data []byte) error {
	var mc MessageCodec
	defer mc.Close()
	return mc.Decode(m, data)
}

func DecodeMessage(data []byte) (m Message, err error) {
	m = NewMessage()
	err = m.Decode(data)
	return
}

// Encode m using buffer. Return the final buffer used to hold m,
// may be different if the initial buffer was not large enough.
func (mc *MessageCodec) Encode(m Message, buffer []byte) ([]byte, error) {
	pn := mc.pnMessage()
	m.(*message).put(pn)
	encode := func(buf []byte) ([]byte, error) {
		len := cLen(buf)
		result := C.pn_message_encode(pn, cPtr(buf), &len)
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

func (m *message) Encode(buffer []byte) ([]byte, error) {
	var mc MessageCodec
	defer mc.Close()
	return mc.Encode(m, buffer)
}

// TODO aconway 2015-09-14: Multi-section messages.

type ignoreFunc func(v interface{}) bool

func isNil(v interface{}) bool   { return v == nil }
func isZero(v interface{}) bool  { return v == reflect.Zero(reflect.TypeOf(v)).Interface() }
func isEmpty(v interface{}) bool { return reflect.ValueOf(v).Len() == 0 }

type stringBuilder struct {
	bytes.Buffer
	separator string
}

func (b *stringBuilder) field(name string, value interface{}, ignore ignoreFunc) {
	if !ignore(value) {
		b.WriteString(b.separator)
		b.separator = ", "
		b.WriteString(name)
		b.WriteString(": ")
		fmt.Fprintf(&b.Buffer, "%v", value)
	}
}

// Human-readable string describing message.
// Includes only message fields with non-default values.
func (m *message) String() string {
	var b stringBuilder
	b.WriteString("Message{")
	b.field("address", m.address, isEmpty)
	b.field("durable", m.durable, isZero)
	// Priority has weird default
	b.field("priority", m.priority, func(v interface{}) bool { return v.(uint8) == 4 })
	b.field("ttl", m.ttl, isZero)
	b.field("first-acquirer", m.firstAcquirer, isZero)
	b.field("delivery-count", m.deliveryCount, isZero)
	b.field("message-id", m.messageId, isNil)
	b.field("user-id", m.userId, isEmpty)
	b.field("subject", m.subject, isEmpty)
	b.field("reply-to", m.replyTo, isEmpty)
	b.field("correlation-id", m.correlationId, isNil)
	b.field("content-type", m.contentType, isEmpty)
	b.field("content-encoding", m.contentEncoding, isEmpty)
	b.field("expiry-time", m.expiryTime, isZero)
	b.field("creation-time", m.creationTime, isZero)
	b.field("group-id", m.groupId, isEmpty)
	b.field("group-sequence", m.groupSequence, isZero)
	b.field("reply-to-group-id", m.replyToGroupId, isEmpty)
	b.field("inferred", m.inferred, isZero)
	b.field("delivery-annotations", m.deliveryAnnotations, isEmpty)
	b.field("message-annotations", m.messageAnnotations, isEmpty)
	b.field("application-properties", m.applicationProperties, isEmpty)
	b.field("body", m.body, isNil)
	b.WriteString("}")
	return b.String()
}

// ==== get message from pn_message_t

func getData(v interface{}, data *C.pn_data_t) {
	if data != nil && C.pn_data_size(data) > 0 {
		C.pn_data_rewind(data)
		C.pn_data_next(data)
		unmarshal(v, data)
	}
	return
}

func getString(c *C.char) string {
	if c == nil {
		return ""
	}
	return C.GoString(c)
}

func (m *message) get(pn *C.pn_message_t) {
	m.Clear()
	m.inferred = bool(C.pn_message_is_inferred(pn))
	m.durable = bool(C.pn_message_is_durable(pn))
	m.priority = uint8(C.pn_message_get_priority(pn))
	m.ttl = goDuration(C.pn_message_get_ttl(pn))
	m.firstAcquirer = bool(C.pn_message_is_first_acquirer(pn))
	m.deliveryCount = uint32(C.pn_message_get_delivery_count(pn))
	getData(&m.messageId, C.pn_message_id(pn))
	m.userId = string(goBytes(C.pn_message_get_user_id(pn)))
	m.address = getString(C.pn_message_get_address(pn))
	m.subject = getString(C.pn_message_get_subject(pn))
	m.replyTo = getString(C.pn_message_get_reply_to(pn))
	getData(&m.correlationId, C.pn_message_correlation_id(pn))
	m.contentType = getString(C.pn_message_get_content_type(pn))
	m.contentEncoding = getString(C.pn_message_get_content_encoding(pn))
	m.expiryTime = goTime(C.pn_message_get_expiry_time(pn))
	m.creationTime = goTime(C.pn_message_get_creation_time(pn))
	m.groupId = getString(C.pn_message_get_group_id(pn))
	m.groupSequence = int32(C.pn_message_get_group_sequence(pn))
	m.replyToGroupId = getString(C.pn_message_get_reply_to_group_id(pn))
	getData(&m.deliveryAnnotations, C.pn_message_instructions(pn))
	getData(&m.messageAnnotations, C.pn_message_annotations(pn))
	getData(&m.applicationProperties, C.pn_message_properties(pn))
	getData(&m.body, C.pn_message_body(pn))
}

// ==== put message to pn_message_t

func putData(v interface{}, pn *C.pn_data_t) {
	if v != nil {
		C.pn_data_clear(pn)
		marshal(v, pn)
	}
}

// For pointer-based fields (pn_data_t, strings, bytes) only
// put a field if it has a non-empty value
func (m *message) put(pn *C.pn_message_t) {
	C.pn_message_clear(pn)
	C.pn_message_set_inferred(pn, C.bool(m.inferred))
	C.pn_message_set_durable(pn, C.bool(m.durable))
	C.pn_message_set_priority(pn, C.uint8_t(m.priority))
	C.pn_message_set_ttl(pn, pnDuration(m.ttl))
	C.pn_message_set_first_acquirer(pn, C.bool(m.firstAcquirer))
	C.pn_message_set_delivery_count(pn, C.uint32_t(m.deliveryCount))
	putData(m.messageId, C.pn_message_id(pn))
	if m.userId != "" {
		C.pn_message_set_user_id(pn, pnBytes(([]byte)(m.userId)))
	}
	if m.address != "" {
		C.pn_message_set_address(pn, C.CString(m.address))
	}
	if m.subject != "" {
		C.pn_message_set_subject(pn, C.CString(m.subject))
	}
	if m.replyTo != "" {
		C.pn_message_set_reply_to(pn, C.CString(m.replyTo))
	}
	putData(m.correlationId, C.pn_message_correlation_id(pn))
	if m.contentType != "" {
		C.pn_message_set_content_type(pn, C.CString(m.contentType))
	}
	if m.contentEncoding != "" {
		C.pn_message_set_content_encoding(pn, C.CString(m.contentEncoding))
	}
	C.pn_message_set_expiry_time(pn, pnTime(m.expiryTime))
	C.pn_message_set_creation_time(pn, pnTime(m.creationTime))
	if m.groupId != "" {
		C.pn_message_set_group_id(pn, C.CString(m.groupId))
	}
	C.pn_message_set_group_sequence(pn, C.pn_sequence_t(m.groupSequence))
	if m.replyToGroupId != "" {
		C.pn_message_set_reply_to_group_id(pn, C.CString(m.replyToGroupId))
	}
	if len(m.deliveryAnnotations) != 0 {
		putData(m.deliveryAnnotations, C.pn_message_instructions(pn))
	}
	if len(m.messageAnnotations) != 0 {
		putData(m.messageAnnotations, C.pn_message_annotations(pn))
	}
	if len(m.applicationProperties) != 0 {
		putData(m.applicationProperties, C.pn_message_properties(pn))
	}
	putData(m.body, C.pn_message_body(pn))
}

// ==== Deprecated functions

func oldAnnotations(in map[AnnotationKey]interface{}) (out map[string]interface{}) {
	if len(in) == 0 {
		return nil
	}
	out = make(map[string]interface{})
	for k, v := range in {
		out[k.String()] = v
	}
	return
}

func (m *message) Instructions() map[string]interface{} {
	return oldAnnotations(m.deliveryAnnotations)
}
func (m *message) Annotations() map[string]interface{} {
	return oldAnnotations(m.messageAnnotations)
}
func (m *message) Properties() map[string]interface{} {
	return m.applicationProperties
}

// Convert old string-keyed annotations to an AnnotationKey map
func newAnnotations(in map[string]interface{}) (out map[AnnotationKey]interface{}) {
	if len(in) == 0 {
		return nil
	}
	out = make(map[AnnotationKey]interface{})
	for k, v := range in {
		out[AnnotationKeyString(k)] = v
	}
	return
}

func (m *message) SetInstructions(v map[string]interface{}) {
	m.deliveryAnnotations = newAnnotations(v)
}
func (m *message) SetAnnotations(v map[string]interface{}) {
	m.messageAnnotations = newAnnotations(v)
}
func (m *message) SetProperties(v map[string]interface{}) {
	m.applicationProperties = v
}
