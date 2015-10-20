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
#include "proton/data.hpp"
#include "proton/export.hpp"
#include "proton/facade.hpp"
#include "proton/pn_unique_ptr.hpp"

#include <string>
#include <utility>

struct pn_message_t;

namespace proton {

class link;
class delivery;

/** An AMQP message. Value semantics, can be copied or assigned to make a new message. */
class message
{
  public:
    PN_CPP_EXTERN message();
    PN_CPP_EXTERN message(const message&);
#if PN_HAS_CPP11
    PN_CPP_EXTERN message(message&&);
#endif
    PN_CPP_EXTERN ~message();
    PN_CPP_EXTERN message& operator=(const message&);

    void swap(message& x);

    /** Clear the message content and properties. */
    PN_CPP_EXTERN void clear();

    ///@name Message properties
    ///@{

    ///@ Set message identifier, can be a string, unsigned long, uuid or binary.
    PN_CPP_EXTERN void id(const data& id);
    ///@ Get message identifier
    PN_CPP_EXTERN data& id();
    ///@ Get message identifier reference, allows modification in-place.
    PN_CPP_EXTERN const data& id() const;

    PN_CPP_EXTERN void user(const std::string &user);
    PN_CPP_EXTERN std::string user() const;

    PN_CPP_EXTERN void address(const std::string &addr);
    PN_CPP_EXTERN std::string address() const;

    PN_CPP_EXTERN void subject(const std::string &s);
    PN_CPP_EXTERN std::string subject() const;

    PN_CPP_EXTERN void reply_to(const std::string &s);
    PN_CPP_EXTERN std::string reply_to() const;

    /// Get correlation identifier, can be a string, unsigned long, uuid or binary.
    PN_CPP_EXTERN void correlation_id(const data&);
    /// Get correlation identifier.
    PN_CPP_EXTERN const data& correlation_id() const;
    /// Get correlation identifier reference, allows modification in-place.
    PN_CPP_EXTERN data& correlation_id();

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

    /** Set the body. If data has more than one value, each is encoded as an AMQP section. */
    PN_CPP_EXTERN void body(const data&);

    /** Set the body to any type T that can be converted to proton::data */
    template <class T> void body(const T& v) { body().clear(); body().encoder() << v; }

    /** Get the body values. */
    PN_CPP_EXTERN const data& body() const;

    /** Get a reference to the body data, can be modified in-place. */
    PN_CPP_EXTERN data& body();

    /** Encode into memory starting at buffer.first and ending before buffer.second */
    PN_CPP_EXTERN void encode(std::pair<char*, char*> buffer);

    /** Encode into a string, growing the string if necessary. */
    PN_CPP_EXTERN void encode(std::string &data) const;

    /** Return encoded message as a string */
    PN_CPP_EXTERN std::string encode() const;

    /** Decode from memory starting at buffer.first and ending before buffer.second */
    PN_CPP_EXTERN void decode(std::pair<const char*, const char*> buffer);

    /** Decode from string data into the message. */
    PN_CPP_EXTERN void decode(const std::string &data);

    /// Decode the message from link corresponding to delivery.
    PN_CPP_EXTERN void decode(proton::link&, proton::delivery&);

  private:
    pn_message_t *message_;
};

}

#endif  /*!PROTON_CPP_MESSAGE_H*/
