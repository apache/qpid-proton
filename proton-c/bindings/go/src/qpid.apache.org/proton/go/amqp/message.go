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

// #include <proton/types.h>
// #include <proton/message.h>
// #include <proton/codec.h>
import "C"

import (
	"qpid.apache.org/proton/go/internal"
	"time"
	"unsafe"
)

// FIXME aconway 2015-04-28: Do we need the interface or can we just export the struct?

// Message is the interface to an AMQP message.
// Instances of this interface contain a pointer to the underlying struct.
type Message interface {
	/**
	 * Inferred indicates how the message content
	 * is encoded into AMQP sections. If inferred is true then binary and
	 * list values in the body of the message will be encoded as AMQP DATA
	 * and AMQP SEQUENCE sections, respectively. If inferred is false,
	 * then all values in the body of the message will be encoded as AMQP
	 * VALUE sections regardless of their type.
	 */
	Inferred() bool
	SetInferred(bool)

	/**
	 * Durable indicates that any parties taking responsibility
	 * for the message must durably store the content.
	 */
	Durable() bool
	SetDurable(bool)

	/**
	 * Priority impacts ordering guarantees. Within a
	 * given ordered context, higher priority messages may jump ahead of
	 * lower priority messages.
	 */
	Priority() uint8
	SetPriority(uint8)

	/**
	 * TTL or Time To Live, a message it may be dropped after this duration
	 */
	TTL() time.Duration
	SetTTL(time.Duration)

	/**
	 * FirstAcquirer indicates
	 * that the recipient of the message is the first recipient to acquire
	 * the message, i.e. there have been no failed delivery attempts to
	 * other acquirers. Note that this does not mean the message has not
	 * been delivered to, but not acquired, by other recipients.
	 */
	FirstAcquirer() bool
	SetFirstAcquirer(bool)

	/**
	 * DeliveryCount tracks how many attempts have been made to
	 * delivery a message.
	 */
	DeliveryCount() uint32
	SetDeliveryCount(uint32)

	/**
	 * MessageId provides a unique identifier for a message.
	 * it can be an a string, an unsigned long, a uuid or a
	 * binary value.
	 */
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

	/**
	 * CorrelationId is set on correlated request and response messages. It can be an a string, an unsigned long, a uuid or a
	 * binary value.
	 */
	CorrelationId() interface{}
	SetCorrelationId(interface{})

	ContentType() string
	SetContentType(string)

	ContentEncoding() string
	SetContentEncoding(string)

	// ExpiryTime indicates an absoulte time when the message may be dropped.
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

	/**
	 * Instructions can be used to access or modify AMQP delivery instructions.
	 */
	Instructions() *map[string]interface{}

	/**
	 * Annotations  can be used to access or modify AMQP annotations.
	 */
	Annotations() *map[string]interface{}

	/**
	 * Properties  can be used to access or modify the application properties of a message.
	 */
	Properties() *map[string]interface{}

	/**
	 * Body of the message can be any AMQP encodable type.
	 */
	Body() interface{}
	SetBody(interface{})

	// Encode encodes the message as AMQP data. If buffer is non-nil and is large enough
	// the message is encoded into it, otherwise a new buffer is created.
	// Returns the buffer containing the message.
	Encode(buffer []byte) ([]byte, error)
}

// NewMessage creates a new message instance. The returned interface contains a pointer.
func NewMessage() Message {
	pn := C.pn_message() // Pick up default setting from C message.
	defer C.pn_message_free(pn)
	return goMessage(pn)
}

// Message implementation copies all message data into Go space so it can be proprely
// memory managed.
//
type message struct {
	inferred, durable, firstAcquirer      bool
	priority                              uint8
	ttl                                   time.Duration
	deliveryCount                         uint32
	messageId                             interface{}
	userId, address, subject, replyTo     string
	contentType, contentEncoding          string
	groupId, replyToGroupId               string
	creationTime, expiryTime              time.Time
	groupSequence                         int32
	correlationId                         interface{}
	instructions, annotations, properties map[string]interface{}
	body                                  interface{}
}

func (m *message) Inferred() bool                        { return m.inferred }
func (m *message) SetInferred(b bool)                    { m.inferred = b }
func (m *message) Durable() bool                         { return m.durable }
func (m *message) SetDurable(b bool)                     { m.durable = b }
func (m *message) Priority() uint8                       { return m.priority }
func (m *message) SetPriority(b uint8)                   { m.priority = b }
func (m *message) TTL() time.Duration                    { return m.ttl }
func (m *message) SetTTL(d time.Duration)                { m.ttl = d }
func (m *message) FirstAcquirer() bool                   { return m.firstAcquirer }
func (m *message) SetFirstAcquirer(b bool)               { m.firstAcquirer = b }
func (m *message) DeliveryCount() uint32                 { return m.deliveryCount }
func (m *message) SetDeliveryCount(c uint32)             { m.deliveryCount = c }
func (m *message) MessageId() interface{}                { return m.messageId }
func (m *message) SetMessageId(id interface{})           { m.messageId = id }
func (m *message) UserId() string                        { return m.userId }
func (m *message) SetUserId(s string)                    { m.userId = s }
func (m *message) Address() string                       { return m.address }
func (m *message) SetAddress(s string)                   { m.address = s }
func (m *message) Subject() string                       { return m.subject }
func (m *message) SetSubject(s string)                   { m.subject = s }
func (m *message) ReplyTo() string                       { return m.replyTo }
func (m *message) SetReplyTo(s string)                   { m.replyTo = s }
func (m *message) CorrelationId() interface{}            { return m.correlationId }
func (m *message) SetCorrelationId(c interface{})        { m.correlationId = c }
func (m *message) ContentType() string                   { return m.contentType }
func (m *message) SetContentType(s string)               { m.contentType = s }
func (m *message) ContentEncoding() string               { return m.contentEncoding }
func (m *message) SetContentEncoding(s string)           { m.contentEncoding = s }
func (m *message) ExpiryTime() time.Time                 { return m.expiryTime }
func (m *message) SetExpiryTime(t time.Time)             { m.expiryTime = t }
func (m *message) CreationTime() time.Time               { return m.creationTime }
func (m *message) SetCreationTime(t time.Time)           { m.creationTime = t }
func (m *message) GroupId() string                       { return m.groupId }
func (m *message) SetGroupId(s string)                   { m.groupId = s }
func (m *message) GroupSequence() int32                  { return m.groupSequence }
func (m *message) SetGroupSequence(s int32)              { m.groupSequence = s }
func (m *message) ReplyToGroupId() string                { return m.replyToGroupId }
func (m *message) SetReplyToGroupId(s string)            { m.replyToGroupId = s }
func (m *message) Instructions() *map[string]interface{} { return &m.instructions }
func (m *message) Annotations() *map[string]interface{}  { return &m.annotations }
func (m *message) Properties() *map[string]interface{}   { return &m.properties }
func (m *message) Body() interface{}                     { return m.body }
func (m *message) SetBody(b interface{})                 { m.body = b }

// rewindGet rewinds and then gets the value from a data object.
func rewindGet(data *C.pn_data_t, v interface{}) {
	if data != nil && C.pn_data_size(data) > 0 {
		C.pn_data_rewind(data)
		C.pn_data_next(data)
		get(data, v)
	}
}

// goMessage populates a Go message from a pn_message_t
func goMessage(pn *C.pn_message_t) *message {
	m := &message{
		inferred:        bool(C.pn_message_is_inferred(pn)),
		durable:         bool(C.pn_message_is_durable(pn)),
		priority:        uint8(C.pn_message_get_priority(pn)),
		ttl:             time.Duration(C.pn_message_get_ttl(pn)) * time.Millisecond,
		firstAcquirer:   bool(C.pn_message_is_first_acquirer(pn)),
		deliveryCount:   uint32(C.pn_message_get_delivery_count(pn)),
		userId:          goString(C.pn_message_get_user_id(pn)),
		address:         C.GoString(C.pn_message_get_address(pn)),
		subject:         C.GoString(C.pn_message_get_subject(pn)),
		replyTo:         C.GoString(C.pn_message_get_reply_to(pn)),
		contentType:     C.GoString(C.pn_message_get_content_type(pn)),
		contentEncoding: C.GoString(C.pn_message_get_content_encoding(pn)),
		expiryTime:      time.Unix(0, int64(time.Millisecond*time.Duration(C.pn_message_get_expiry_time(pn)))),
		creationTime:    time.Unix(0, int64(time.Millisecond)*int64(C.pn_message_get_creation_time(pn))),
		groupId:         C.GoString(C.pn_message_get_group_id(pn)),
		groupSequence:   int32(C.pn_message_get_group_sequence(pn)),
		replyToGroupId:  C.GoString(C.pn_message_get_reply_to_group_id(pn)),
		messageId:       nil,
		correlationId:   nil,
		instructions:    make(map[string]interface{}),
		annotations:     make(map[string]interface{}),
		properties:      make(map[string]interface{}),
	}
	rewindGet(C.pn_message_id(pn), &m.messageId)
	rewindGet(C.pn_message_correlation_id(pn), &m.correlationId)
	rewindGet(C.pn_message_instructions(pn), &m.instructions)
	rewindGet(C.pn_message_annotations(pn), &m.annotations)
	rewindGet(C.pn_message_properties(pn), &m.properties)
	rewindGet(C.pn_message_body(pn), &m.body)
	return m
}

// pnMessage populates a pn_message_t from a Go message.
func (m *message) pnMessage() *C.pn_message_t {
	pn := C.pn_message()
	C.pn_message_set_inferred(pn, C.bool(m.Inferred()))
	C.pn_message_set_durable(pn, C.bool(m.Durable()))
	C.pn_message_set_priority(pn, C.uint8_t(m.priority))
	C.pn_message_set_ttl(pn, C.pn_millis_t(m.TTL()/time.Millisecond))
	C.pn_message_set_first_acquirer(pn, C.bool(m.FirstAcquirer()))
	C.pn_message_set_delivery_count(pn, C.uint32_t(m.deliveryCount))
	replace(C.pn_message_id(pn), m.MessageId())
	C.pn_message_set_user_id(pn, pnBytes([]byte(m.UserId())))
	C.pn_message_set_address(pn, C.CString(m.Address()))
	C.pn_message_set_subject(pn, C.CString(m.Subject()))
	C.pn_message_set_reply_to(pn, C.CString(m.ReplyTo()))
	replace(C.pn_message_correlation_id(pn), m.CorrelationId())
	C.pn_message_set_content_type(pn, C.CString(m.ContentType()))
	C.pn_message_set_content_encoding(pn, C.CString(m.ContentEncoding()))
	C.pn_message_set_expiry_time(pn, pnTime(m.ExpiryTime()))
	C.pn_message_set_creation_time(pn, pnTime(m.CreationTime()))
	C.pn_message_set_group_id(pn, C.CString(m.GroupId()))
	C.pn_message_set_group_sequence(pn, C.pn_sequence_t(m.GroupSequence()))
	C.pn_message_set_reply_to_group_id(pn, C.CString(m.ReplyToGroupId()))
	replace(C.pn_message_instructions(pn), *m.Instructions())
	replace(C.pn_message_annotations(pn), *m.Annotations())
	replace(C.pn_message_properties(pn), *m.Properties())
	replace(C.pn_message_body(pn), m.Body())
	return pn
}

// FIXME aconway 2015-04-08: Move message encode/decode under Marshal/Unmarshal interfaces.

// DecodeMessage decodes bytes as a message
func DecodeMessage(data []byte) (Message, error) {
	pnMsg := C.pn_message()
	defer C.pn_message_free(pnMsg)
	if len(data) == 0 {
		return nil, internal.Errorf("empty buffer for decode")
	}
	if C.pn_message_decode(pnMsg, cPtr(data), cLen(data)) < 0 {
		return nil, internal.Errorf("decoding message: %s",
			internal.PnError(unsafe.Pointer(C.pn_message_error(pnMsg))))
	}
	return goMessage(pnMsg), nil
}

// Encode the message into bufffer.
// If buffer is nil or len(buffer) is not sufficient to encode the message a larger
// buffer will be returned.
func (m *message) Encode(buffer []byte) ([]byte, error) {
	pn := m.pnMessage()
	defer C.pn_message_free(pn)
	encode := func(buf []byte) ([]byte, error) {
		len := cLen(buf)
		result := C.pn_message_encode(pn, cPtr(buf), &len)
		switch {
		case result == C.PN_OVERFLOW:
			return buf, overflow
		case result < 0:
			return buf, internal.Errorf("cannot encode message: %s", internal.PnErrorCode(result))
		default:
			return buf[:len], nil
		}
	}
	return encodeGrow(buffer, encode)
}
