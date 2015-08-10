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
#include "proton/value.hpp"
#include "proton/message.hpp"
#include <string>

struct pn_message_t;

namespace proton {

class link;
class delivery;

/** An AMQP message.
 *
 * This class has value semantics: the copy constructor and assignment make a
 * copy of the underlying message data. If you want to transfer the message data
 * without a copy use the swap member function or std::swap.
 */

class message
{
  public:
    PN_CPP_EXTERN message();
    /** Takes ownership of the pn_message_t, calls pn_message_free on destruction. */
    PN_CPP_EXTERN message(pn_message_t *);
    /// Makes a copy of the other message.
    PN_CPP_EXTERN message(const message&);
    /// Makes a copy of the other message.
    PN_CPP_EXTERN message& operator=(const message&);

    PN_CPP_EXTERN ~message();

    PN_CPP_EXTERN void swap(message&);

    /// Access the underlying pn_message_t, note it will be freed by ~message.
    PN_CPP_EXTERN pn_message_t *pn_message();
    /// Forget the underlying pn_message_t, the message is cleared. Caller must call pn_message_free.
    PN_CPP_EXTERN pn_message_t *pn_message_forget();

    /** Clear the message content */
    PN_CPP_EXTERN void clear();

    ///@name Message properties
    ///@{

    /// Globally unique identifier, can be an a string, an unsigned long, a uuid or a binary value.
    PN_CPP_EXTERN void id(const value& id);
    /// Globally unique identifier, can be an a string, an unsigned long, a uuid or a binary value.
    PN_CPP_EXTERN value id() const;

    PN_CPP_EXTERN void user(const std::string &user);
    PN_CPP_EXTERN std::string user() const;

    PN_CPP_EXTERN void address(const std::string &addr);
    PN_CPP_EXTERN std::string address() const;

    PN_CPP_EXTERN void subject(const std::string &s);
    PN_CPP_EXTERN std::string subject() const;

    PN_CPP_EXTERN void reply_to(const std::string &s);
    PN_CPP_EXTERN std::string reply_to() const;

    /// Correlation identifier, can be an a string, an unsigned long, a uuid or a binary value.
    PN_CPP_EXTERN void correlation_id(const value&);
    /// Correlation identifier, can be an a string, an unsigned long, a uuid or a binary value.
    PN_CPP_EXTERN value correlation_id() const;

    PN_CPP_EXTERN void content_type(const std::string &s);
    PN_CPP_EXTERN std::string content_type() const;

    PN_CPP_EXTERN void content_encoding(const std::string &s);
    PN_CPP_EXTERN std::string content_encoding() const;

    PN_CPP_EXTERN void expiry(amqp_timestamp t);
    PN_CPP_EXTERN amqp_timestamp expiry() const;

    PN_CPP_EXTERN void creation_time(amqp_timestamp t);
    PN_CPP_EXTERN amqp_timestamp creation_time() const;

    PN_CPP_EXTERN void group_id(const std::string &s);
    PN_CPP_EXTERN std::string group_id() const;

    PN_CPP_EXTERN void reply_to_group_id(const std::string &s);
    PN_CPP_EXTERN std::string reply_to_group_id() const;
    ///@}

    /** Set the body to a proton::value, copies the value. */
    PN_CPP_EXTERN void body(const value&);

    /** Set the body to any type T that can be converted to a proton::value */
    template <class T> void body(const T& v) { body(value(v)); }

    /** Set the body to a sequence of values, each value is encoded as an AMQP section. */
    PN_CPP_EXTERN void body(const values&);

    /** Get the body values, there may be more than one.
     * Note the reference will be invalidated by destroying the message or calling swap.
     */
    PN_CPP_EXTERN const values& body() const;

    /** Get a reference to the body values, can be modified in-place.
     * Note the reference will be invalidated by destroying the message or calling swap.
     */
    PN_CPP_EXTERN values& body();

    // TODO aconway 2015-06-17: consistent and flexible treatment of buffers.
    // Allow convenient std::string encoding/decoding (with re-use of existing
    // string capacity) but also need to allow encoding/decoding of non-string
    // buffers. Introduce a buffer type with begin/end pointers?

    /** Encode the message into string data */
    PN_CPP_EXTERN void encode(std::string &data) const;
    /** Retrun encoded message as a string */
    PN_CPP_EXTERN std::string encode() const;
    /** Decode from string data into the message. */
    PN_CPP_EXTERN void decode(const std::string &data);

    /// Decode the message from link corresponding to delivery.
    PN_CPP_EXTERN void decode(proton::link link, proton::delivery);

  private:
    pn_message_t *impl_;
    mutable values body_;
};

}

#endif  /*!PROTON_CPP_MESSAGE_H*/
