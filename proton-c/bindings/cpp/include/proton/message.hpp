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
#include "proton/proton_handle.hpp"
#include "proton/value.hpp"
#include "proton/message.hpp"
#include <string>

struct pn_message_t;
struct pn_data_t;

namespace proton {

// TODO aconway 2015-08-07: make this a value-semantics class, hide pn_message_t.

/// An AMQP message.
class message : public proton_handle<pn_message_t>
{
  public:
    PN_CPP_EXTERN message();
    PN_CPP_EXTERN message(pn_message_t *);
    PN_CPP_EXTERN message(const message&);
    PN_CPP_EXTERN message& operator=(const message&);
    PN_CPP_EXTERN ~message();

    PN_CPP_EXTERN pn_message_t *pn_message() const;

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

    /** Set the body to a proton::value. */
    PN_CPP_EXTERN void body(const value&);

    /** Set the body to any type T that can be converted to a proton::value */
    template <class T> void body(const T& v) { body(value(v)); }

    /** Set the body to a sequence of values, each value is encoded as an AMQP section. */
    PN_CPP_EXTERN void body(const values&);

    /** Get the body values, there may be more than one. */
    PN_CPP_EXTERN const values& body() const;

    /** Get a reference to the body, can be modified in-place. */
    PN_CPP_EXTERN values& body();

    // TODO aconway 2015-06-17: consistent and flexible treatment of buffers.
    // Allow convenient std::string encoding/decoding (with re-use of existing
    // string capacity) but also need to allow encoding/decoding of non-string
    // buffers. Introduce a buffer type with begin/end pointers?

    /** Encode the message into string data */
    PN_CPP_EXTERN void encode(std::string &data);
    /** Retrun encoded message as a string */
    PN_CPP_EXTERN std::string encode();
    /** Decode from string data into the message. */
    PN_CPP_EXTERN void decode(const std::string &data);

  private:
    mutable values body_;
  friend class proton_impl_ref<message>;
};

}

#endif  /*!PROTON_CPP_MESSAGE_H*/
