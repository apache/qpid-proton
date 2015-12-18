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

#include "proton/export.hpp"
#include "proton/message_id.hpp"
#include "proton/annotation_key.hpp"
#include "proton/pn_unique_ptr.hpp"
#include "proton/value.hpp"
#include "proton/duration.hpp"

#include <string>
#include <utility>

struct pn_message_t;

namespace proton {

class link;
class delivery;
class message_id;
class annotation_key;

/** An AMQP message. Value semantics, can be copied or assigned to make a new message. */
class message
{
  public:
    typedef std::map<std::string, scalar> property_map;
    typedef std::map<annotation_key, value> annotation_map;

    PN_CPP_EXTERN message();
    PN_CPP_EXTERN message(const message&);
#if PN_HAS_CPP11
    PN_CPP_EXTERN message(message&&);
#endif
    /// Constructor that sets the body from any type that can be assigned to a value.
    template <class T> explicit message(const T& body_) { body() = body_; }

    PN_CPP_EXTERN ~message();

    PN_CPP_EXTERN message& operator=(const message&);

    PN_CPP_EXTERN void swap(message& x);

    /** Clear the message content and properties. */
    PN_CPP_EXTERN void clear();

    ///@name Standard AMQP properties
    ///@{

    PN_CPP_EXTERN void id(const message_id& id);
    PN_CPP_EXTERN message_id id() const;

    PN_CPP_EXTERN void user_id(const std::string &user);
    PN_CPP_EXTERN std::string user_id() const;

    PN_CPP_EXTERN void address(const std::string &addr);
    PN_CPP_EXTERN std::string address() const;

    PN_CPP_EXTERN void subject(const std::string &s);
    PN_CPP_EXTERN std::string subject() const;

    PN_CPP_EXTERN void reply_to(const std::string &s);
    PN_CPP_EXTERN std::string reply_to() const;

    PN_CPP_EXTERN void correlation_id(const message_id&);
    PN_CPP_EXTERN message_id correlation_id() const;

    PN_CPP_EXTERN void content_type(const std::string &s);
    PN_CPP_EXTERN std::string content_type() const;

    PN_CPP_EXTERN void content_encoding(const std::string &s);
    PN_CPP_EXTERN std::string content_encoding() const;

    PN_CPP_EXTERN void expiry_time(amqp_timestamp t);
    PN_CPP_EXTERN amqp_timestamp expiry_time() const;

    PN_CPP_EXTERN void creation_time(amqp_timestamp t);
    PN_CPP_EXTERN amqp_timestamp creation_time() const;

    PN_CPP_EXTERN void group_id(const std::string &s);
    PN_CPP_EXTERN std::string group_id() const;

    PN_CPP_EXTERN void reply_to_group_id(const std::string &s);
    PN_CPP_EXTERN std::string reply_to_group_id() const;

    ///@}

    /** Set the body, equivalent to body() = v */
    template<class T> void body(const T& v) { body() = v; }

    /** Get the body. */
    PN_CPP_EXTERN const value& body() const;

    /** Get a reference to the body that can be modified in-place. */
    PN_CPP_EXTERN value& body();

    /** Application properties map, can be modified in place. */
    PN_CPP_EXTERN property_map& properties();
    PN_CPP_EXTERN const property_map& properties() const;

    /** Message annotations map, can be modified in place. */
    PN_CPP_EXTERN annotation_map& annotations();
    PN_CPP_EXTERN const annotation_map& annotations() const;

    /** Delivery instructions map, can be modified in place. */
    PN_CPP_EXTERN annotation_map& instructions();
    PN_CPP_EXTERN const annotation_map& instructions() const;

    /** Encode entire message into a string, growing the string if necessary. */
    PN_CPP_EXTERN void encode(std::string &bytes) const;

    /** Return encoded message as a string */
    PN_CPP_EXTERN std::string encode() const;

    /** Decode from string data into the message. */
    PN_CPP_EXTERN void decode(const std::string &bytes);

    /** Decode the message corresponding to a delivery from a link. */
    PN_CPP_EXTERN void decode(proton::link, proton::delivery);

    /**
     * Get the inferred flag for a message.
     *
     * The inferred flag for a message indicates how the message content
     * is encoded into AMQP sections. If inferred is true then binary and
     * list values in the body of the message will be encoded as AMQP DATA
     * and AMQP SEQUENCE sections, respectively. If inferred is false,
     * then all values in the body of the message will be encoded as AMQP
     * VALUE sections regardless of their type.
     */
    PN_CPP_EXTERN bool inferred() const;
    /** Get the inferred flag for a message. */
    PN_CPP_EXTERN void inferred(bool);

    /**
     * Get the durable flag for a message.
     *
     * The durable flag indicates that any parties taking responsibility
     * for the message must durably store the content.
     *
     * @return the value of the durable flag
     */
    PN_CPP_EXTERN bool durable() const;
    /** Get the durable flag for a message. */
    PN_CPP_EXTERN void durable(bool);

    /**
     * Get the ttl for a message.
     *
     * The ttl for a message determines how long a message is considered
     * live. When a message is held for retransmit, the ttl is
     * decremented. Once the ttl reaches zero, the message is considered
     * dead. Once a message is considered dead it may be dropped.
     *
     * @return the ttl in milliseconds
     */
    PN_CPP_EXTERN duration ttl() const;
    /** Set the ttl for a message */
    PN_CPP_EXTERN void ttl(duration);

    /**
     * Get the priority for a message.
     *
     * The priority of a message impacts ordering guarantees. Within a
     * given ordered context, higher priority messages may jump ahead of
     * lower priority messages.
     *
     * @return the message priority
     */
    PN_CPP_EXTERN uint8_t priority() const;
    /** Get the priority for a message. */
    PN_CPP_EXTERN void priority(uint8_t);

    /**
     * Get the first acquirer flag for a message.
     *
     * When set to true, the first acquirer flag for a message indicates
     * that the recipient of the message is the first recipient to acquire
     * the message, i.e. there have been no failed delivery attempts to
     * other acquirers. Note that this does not mean the message has not
     * been delivered to, but not acquired, by other recipients.
     *
     * @return the first acquirer flag for the message
     */
    PN_CPP_EXTERN bool first_acquirer() const;
    /** Get the first acquirer flag for a message. */
    PN_CPP_EXTERN void first_acquirer(bool);

    /**
     * Get the delivery count for a message.
     *
     * The delivery count field tracks how many attempts have been made to
     * delivery a message.
     *
     * @return the delivery count for the message
     */
    PN_CPP_EXTERN uint32_t delivery_count() const;
    /** Get the delivery count for a message. */
    PN_CPP_EXTERN void delivery_count(uint32_t);

    /**
     * Get the group sequence for a message.
     *
     * The group sequence of a message identifies the relative ordering of
     * messages within a group. The default value for the group sequence
     * of a message is zero.
     *
     * @return the group sequence for the message
     */
    PN_CPP_EXTERN int32_t sequence() const;
    /** Get the group sequence for a message. */
    PN_CPP_EXTERN void sequence(int32_t);

  private:
    pn_message_t *pn_msg() const;
    struct impl {
        PN_CPP_EXTERN impl();
        PN_CPP_EXTERN ~impl();
        mutable pn_message_t *msg;
        mutable value body;
    } impl_;
    mutable property_map properties_;
    mutable annotation_map annotations_;
    mutable annotation_map instructions_;
};

}

#endif  /*!PROTON_CPP_MESSAGE_H*/
