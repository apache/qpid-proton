#ifndef PROTON_CPP_MESSAGE_H
#define PROTON_CPP_MESSAGE_H

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

#include <proton/map.hpp>
#include <proton/annotation_key.hpp>
#include <proton/duration.hpp>
#include <proton/export.hpp>
#include <proton/message_id.hpp>
#include <proton/pn_unique_ptr.hpp>
#include <proton/value.hpp>

#include <string>
#include <vector>
#include <utility>

struct pn_message_t;

namespace proton {

class link;
class delivery;
class message_id;
class annotation_key;

/// An AMQP message.
///
/// Value semantics: can be copied or assigned to make a new message.
class message {
  public:
    /// A map of string keys and AMQP scalar values.
    typedef std::map<std::string, scalar> property_map;

    /// A map of AMQP annotation keys and AMQP values.
    typedef std::map<annotation_key, value> annotation_map;

    /// Create an empty message.
    PN_CPP_EXTERN message();

    /// Copy a message.
    PN_CPP_EXTERN message(const message&);

#if PN_CPP_HAS_CPP11
    /// Move a message.
    PN_CPP_EXTERN message(message&&);

    // XXX move assignment operator? - do this in general for CPP11
#endif

    /// Create a message with its body set from any value that can be
    /// converted to a proton::value.
    PN_CPP_EXTERN message(const value& x);

    PN_CPP_EXTERN ~message();

    /// Copy a message.
    PN_CPP_EXTERN message& operator=(const message&);

    /// @name Basic properties and methods
    /// @{

    /// Clear the message content and properties.
    PN_CPP_EXTERN void clear();

    PN_CPP_EXTERN void id(const message_id& id);
    PN_CPP_EXTERN message_id id() const;

    /// @cond INTERNAL
    /// XXX consider just user, in order to be consistent with similar
    /// fields elsewhere in the API
    /// XXX ask gordon about use case - decision sort of: "user" instead of "user_id"
    PN_CPP_EXTERN void user_id(const std::string &user);
    PN_CPP_EXTERN std::string user_id() const;
    /// @endcond

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

    PN_CPP_EXTERN void address(const std::string &addr);
    PN_CPP_EXTERN std::string address() const;

    PN_CPP_EXTERN void reply_to(const std::string &addr);
    PN_CPP_EXTERN std::string reply_to() const;

    PN_CPP_EXTERN void correlation_id(const message_id&);
    PN_CPP_EXTERN message_id correlation_id() const;

    /// @}

    /// @name Content
    /// @{

    /// Set the body, equivalent to body() = x
    PN_CPP_EXTERN void body(const value& x);

    /// Get the body.
    PN_CPP_EXTERN const value& body() const;

    /// Get a reference to the body that can be modified in-place.
    PN_CPP_EXTERN value& body();

    PN_CPP_EXTERN void subject(const std::string &s);
    PN_CPP_EXTERN std::string subject() const;

    PN_CPP_EXTERN void content_type(const std::string &s);
    PN_CPP_EXTERN std::string content_type() const;

    PN_CPP_EXTERN void content_encoding(const std::string &s);
    PN_CPP_EXTERN std::string content_encoding() const;

    PN_CPP_EXTERN void expiry_time(timestamp t);
    PN_CPP_EXTERN timestamp expiry_time() const;

    PN_CPP_EXTERN void creation_time(timestamp t);
    PN_CPP_EXTERN timestamp creation_time() const;

    /// Get the inferred flag for a message.
    ///
    /// The inferred flag for a message indicates how the message
    /// content is encoded into AMQP sections. If inferred is true
    /// then binary and list values in the body of the message will be
    /// encoded as AMQP DATA and AMQP SEQUENCE sections,
    /// respectively. If inferred is false, then all values in the
    /// body of the message will be encoded as AMQP VALUE sections
    /// regardless of their type.
    PN_CPP_EXTERN bool inferred() const;

    /// Set the inferred flag for a message.
    PN_CPP_EXTERN void inferred(bool);

    /// @}

    /// @name Transfer headers
    /// @{

    /// Get the durable flag for a message.
    ///
    /// The durable flag indicates that any parties taking
    /// responsibility for the message must durably store the content.
    ///
    /// @return the value of the durable flag
    PN_CPP_EXTERN bool durable() const;
    /// Set the durable flag for a message.
    PN_CPP_EXTERN void durable(bool);

    /// Get the TTL for a message.
    ///
    /// The TTL (time to live) for a message determines how long a
    /// message is considered live. When a message is held for
    /// retransmit, the TTL is decremented. Once the TTL reaches zero,
    /// the message is considered dead. Once a message is considered
    /// dead it may be dropped.
    PN_CPP_EXTERN duration ttl() const;

    /// Set the TTL for a message.
    PN_CPP_EXTERN void ttl(duration);

    /// Get the priority for a message.
    ///
    /// The priority of a message impacts ordering guarantees. Within
    /// a given ordered context, higher priority messages may jump
    /// ahead of lower priority messages.
    PN_CPP_EXTERN uint8_t priority() const;

    /// Set the priority for a message.
    PN_CPP_EXTERN void priority(uint8_t);

    /// Get the first acquirer flag for a message.
    ///
    /// When set to true, the first acquirer flag for a message
    /// indicates that the recipient of the message is the first
    /// recipient to acquire the message, i.e. there have been no
    /// failed delivery attempts to other acquirers.  Note that this
    /// does not mean the message has not been delivered to, but not
    /// acquired, by other recipients.
    PN_CPP_EXTERN bool first_acquirer() const;

    /// Set the first acquirer flag for a message.
    PN_CPP_EXTERN void first_acquirer(bool);

    /// Get the delivery count for a message.
    ///
    /// The delivery count field tracks how many attempts have been made to
    /// delivery a message.
    PN_CPP_EXTERN uint32_t delivery_count() const;

    /// Get the delivery count for a message.
    PN_CPP_EXTERN void delivery_count(uint32_t);

    /// @}

    /// @name Message groups
    /// @{

    PN_CPP_EXTERN void group_id(const std::string &s);
    PN_CPP_EXTERN std::string group_id() const;

    PN_CPP_EXTERN void reply_to_group_id(const std::string &s);
    PN_CPP_EXTERN std::string reply_to_group_id() const;

    /// Get the group sequence for a message.
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

    /// Application properties map, can be modified in place.
    PN_CPP_EXTERN property_map& application_properties();
    PN_CPP_EXTERN const property_map& application_properties() const;

    /// Message annotations map, can be modified in place.
    PN_CPP_EXTERN annotation_map& message_annotations();
    PN_CPP_EXTERN const annotation_map& message_annotations() const;

    /// Delivery annotations map, can be modified in place.
    PN_CPP_EXTERN annotation_map& delivery_annotations();
    PN_CPP_EXTERN const annotation_map& delivery_annotations() const;

    /// @}

    /// @cond INTERNAL
  private:
    pn_message_t *pn_msg() const;

    mutable pn_message_t *pn_msg_;
    mutable value body_;
    mutable property_map application_properties_;
    mutable annotation_map message_annotations_;
    mutable annotation_map delivery_annotations_;

    /// Decode the message corresponding to a delivery from a link.
    void decode(proton::delivery);

    PN_CPP_EXTERN friend void swap(message&, message&);
    friend class messaging_adapter;
    /// @endcond
};

}

#endif // PROTON_CPP_MESSAGE_H
