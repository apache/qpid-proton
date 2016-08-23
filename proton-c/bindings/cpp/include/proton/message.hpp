#ifndef PROTON_MESSAGE_HPP
#define PROTON_MESSAGE_HPP

/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */

#include "./annotation_key.hpp"
#include "./codec/map.hpp"
#include "./duration.hpp"
#include "./internal/export.hpp"
#include "./message_id.hpp"
#include "./value.hpp"

#include "./internal/cached_map.hpp"
#include "./internal/pn_unique_ptr.hpp"

#include <string>
#include <vector>
#include <utility>

struct pn_message_t;

namespace proton {

class delivery;
class message_id;
class annotation_key;

/// An AMQP message.
///
/// Value semantics: A message can be copied or assigned to make a new
/// message.
class message {
  public:
    /// **Experimental** - A map of string keys and AMQP scalar
    /// values.
    class property_map : public internal::cached_map<std::string, scalar> {};

    /// **Experimental** - A map of AMQP annotation keys and AMQP
    /// values.
    class annotation_map : public internal::cached_map<annotation_key, value> {};

    /// Create an empty message.
    PN_CPP_EXTERN message();

    /// Copy a message.
    PN_CPP_EXTERN message(const message&);

    /// Copy a message.
    PN_CPP_EXTERN message& operator=(const message&);

#if PN_CPP_HAS_RVALUE_REFERENCES
    /// Move a message.
    PN_CPP_EXTERN message(message&&);

    /// Move a message.
    PN_CPP_EXTERN message& operator=(message&&);
#endif

    /// Create a message with its body set from any value that can be
    /// converted to a proton::value.
    PN_CPP_EXTERN message(const value& x);

    PN_CPP_EXTERN ~message();

    /// @name Basic properties and methods
    /// @{

    /// Clear the message content and properties.
    PN_CPP_EXTERN void clear();

    /// Set the message ID.
    ///
    /// The message ID uniquely identifies a message within a
    /// messaging system.
    PN_CPP_EXTERN void id(const message_id& id);

    /// Get the message ID.
    PN_CPP_EXTERN message_id id() const;

    /// Set the user name or ID.
    PN_CPP_EXTERN void user(const std::string &user);

    /// Get the user name or ID.
    PN_CPP_EXTERN std::string user() const;

    /// Encode entire message into a byte vector, growing it if
    /// necessary.
    PN_CPP_EXTERN void encode(std::vector<char> &bytes) const;

    /// Return encoded message as a byte vector.
    PN_CPP_EXTERN std::vector<char> encode() const;

    /// Decode from string data into the message.
    PN_CPP_EXTERN void decode(const std::vector<char> &bytes);

    /// @}

    /// @name Routing
    /// @{

    /// Set the destination address.
    PN_CPP_EXTERN void to(const std::string &addr);

    /// Get the destination address.
    PN_CPP_EXTERN std::string to() const;

    /// @cond INTERNAL
    /// These are aliases for to()
    PN_CPP_EXTERN void address(const std::string &addr);
    PN_CPP_EXTERN std::string address() const;
    /// @endcond

    /// Set the address for replies.
    PN_CPP_EXTERN void reply_to(const std::string &addr);

    /// Get the address for replies.
    PN_CPP_EXTERN std::string reply_to() const;

    /// Set the ID for matching related messages.
    PN_CPP_EXTERN void correlation_id(const message_id&);

    /// Get the ID for matching related messages.
    PN_CPP_EXTERN message_id correlation_id() const;

    /// @}

    /// @name Content
    /// @{

    /// Set the body.  Equivalent to `body() = x`.
    PN_CPP_EXTERN void body(const value& x);

    /// Get the body.
    PN_CPP_EXTERN const value& body() const;

    /// Get a reference to the body that can be modified in place.
    PN_CPP_EXTERN value& body();

    /// Set the subject.
    PN_CPP_EXTERN void subject(const std::string &s);

    /// Get the subject.
    PN_CPP_EXTERN std::string subject() const;

    /// Set the content type of the body.
    PN_CPP_EXTERN void content_type(const std::string &s);

    /// Get the content type of the body.
    PN_CPP_EXTERN std::string content_type() const;

    /// Set the content encoding of the body.
    PN_CPP_EXTERN void content_encoding(const std::string &s);

    /// Get the content encoding of the body.
    PN_CPP_EXTERN std::string content_encoding() const;

    /// Set the expiration time.
    PN_CPP_EXTERN void expiry_time(timestamp t);

    /// Get the expiration time.
    PN_CPP_EXTERN timestamp expiry_time() const;

    /// Set the creation time.
    PN_CPP_EXTERN void creation_time(timestamp t);

    /// Get the creation time.
    PN_CPP_EXTERN timestamp creation_time() const;

    /// Get the inferred flag.
    ///
    /// The inferred flag for a message indicates how the message
    /// content is encoded into AMQP sections. If the inferred is true
    /// then binary and list values in the body of the message will be
    /// encoded as AMQP DATA and AMQP SEQUENCE sections,
    /// respectively. If inferred is false, then all values in the
    /// body of the message will be encoded as AMQP VALUE sections
    /// regardless of their type.
    PN_CPP_EXTERN bool inferred() const;

    /// Set the inferred flag.
    PN_CPP_EXTERN void inferred(bool);

    /// @}

    /// @name Transfer headers
    /// @{

    /// Get the durable flag.
    ///
    /// The durable flag indicates that any parties taking
    /// responsibility for the message must durably store the content.
    PN_CPP_EXTERN bool durable() const;

    /// Set the durable flag.
    PN_CPP_EXTERN void durable(bool);

    /// Get the TTL.
    ///
    /// The TTL (time to live) for a message determines how long a
    /// message is considered live. When a message is held for
    /// retransmit, the TTL is decremented. Once the TTL reaches zero,
    /// the message is considered dead. Once a message is considered
    /// dead, it may be dropped.
    PN_CPP_EXTERN duration ttl() const;

    /// Set the TTL.
    PN_CPP_EXTERN void ttl(duration);

    /// Get the priority.
    ///
    /// The priority of a message impacts ordering guarantees. Within
    /// a given ordered context, higher priority messages may jump
    /// ahead of lower priority messages.
    ///
    /// The default value set on newly constructed messages is message::default_priority.
    PN_CPP_EXTERN uint8_t priority() const;

    /// Set the priority.
    PN_CPP_EXTERN void priority(uint8_t);

    /// Get the first acquirer flag.
    ///
    /// When set to true, the first acquirer flag for a message
    /// indicates that the recipient of the message is the first
    /// recipient to acquire the message, i.e. there have been no
    /// failed delivery attempts to other acquirers.  Note that this
    /// does not mean the message has not been delivered to, but not
    /// acquired, by other recipients.

    // XXX The triple-not in the last sentence above is confusing.
    
    PN_CPP_EXTERN bool first_acquirer() const;

    /// Set the first acquirer flag.
    PN_CPP_EXTERN void first_acquirer(bool);

    /// Get the delivery count.
    ///
    /// The delivery count field tracks how many attempts have been
    /// made to deliver a message.
    PN_CPP_EXTERN uint32_t delivery_count() const;

    /// Get the delivery count.
    PN_CPP_EXTERN void delivery_count(uint32_t);

    /// @}

    /// @name Message groups
    /// @{

    /// Set the message group ID.
    PN_CPP_EXTERN void group_id(const std::string &s);

    /// Get the message group ID.
    PN_CPP_EXTERN std::string group_id() const;

    /// Set the reply-to group ID.
    PN_CPP_EXTERN void reply_to_group_id(const std::string &s);

    /// Get the reply-to group ID.
    PN_CPP_EXTERN std::string reply_to_group_id() const;

    /// Get the group sequence.
    ///
    /// The group sequence of a message identifies the relative
    /// ordering of messages within a group. The default value for the
    /// group sequence of a message is zero.
    PN_CPP_EXTERN int32_t group_sequence() const;

    /// Set the group sequence for a message.
    PN_CPP_EXTERN void group_sequence(int32_t);

    /// @}

    /// @name Extended attributes
    /// @{

    /// **Experimental** - Get the application properties map.  It can
    /// be modified in place.
    PN_CPP_EXTERN property_map& properties();

    /// **Experimental** - Get the application properties map.  It can
    /// be modified in place.
    PN_CPP_EXTERN const property_map& properties() const;

    /// **Experimental** - Get the message annotations map.  It can be
    /// modified in place.
    PN_CPP_EXTERN annotation_map& message_annotations();

    /// **Experimental** - Get the message annotations map.  It can be
    /// modified in place.
    PN_CPP_EXTERN const annotation_map& message_annotations() const;

    /// **Experimental** - Get the delivery annotations map.  It can
    /// be modified in place.
    PN_CPP_EXTERN annotation_map& delivery_annotations();

    /// **Experimental** - Get the delivery annotations map.  It can
    /// be modified in place.
    PN_CPP_EXTERN const annotation_map& delivery_annotations() const;

    /// @}

    /// Default priority assigned to new messages.
    PN_CPP_EXTERN static const uint8_t default_priority;

    /// @cond INTERNAL
  private:
    pn_message_t *pn_msg() const;

    mutable pn_message_t *pn_msg_;
    mutable internal::value_ref body_;
    mutable property_map application_properties_;
    mutable annotation_map message_annotations_;
    mutable annotation_map delivery_annotations_;

    /// Decode the message corresponding to a delivery from a link.
    void decode(proton::delivery);

  PN_CPP_EXTERN friend void swap(message&, message&);
  friend class messaging_adapter;
    /// @endcond
};

} // proton

#endif // PROTON_MESSAGE_HPP
