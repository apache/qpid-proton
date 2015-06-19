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

// FIXME aconway 2015-06-17: documentation of properties.
class message : public proton_handle<pn_message_t>
{
  public:
    PN_CPP_EXTERN message();
    PN_CPP_EXTERN message(pn_message_t *);
    PN_CPP_EXTERN message(const message&);
    PN_CPP_EXTERN message& operator=(const message&);
    PN_CPP_EXTERN ~message();

    PN_CPP_EXTERN pn_message_t *pn_message() const;

    PN_CPP_EXTERN void id(const value& id);
    PN_CPP_EXTERN value id() const;

    PN_CPP_EXTERN void user(const std::string &user);
    PN_CPP_EXTERN std::string user() const;

    PN_CPP_EXTERN void address(const std::string &addr);
    PN_CPP_EXTERN std::string address() const;

    PN_CPP_EXTERN void subject(const std::string &s);
    PN_CPP_EXTERN std::string subject() const;

    PN_CPP_EXTERN void reply_to(const std::string &s);
    PN_CPP_EXTERN std::string reply_to() const;

    PN_CPP_EXTERN void correlation_id(const value&);
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

    PN_CPP_EXTERN void reply_togroup_id(const std::string &s);
    PN_CPP_EXTERN std::string reply_togroup_id() const;

    /** Set the body to an AMQP value. */
    PN_CPP_EXTERN void body(const value&);

    /** Template to convert any type to a value and set as the body */
    template <class T> void body(const T& v) { body(value(v)); }

    /** Set the body to a sequence of sections containing AMQP values. */
    PN_CPP_EXTERN void body(const values&);

    PN_CPP_EXTERN const values& body() const;

    PN_CPP_EXTERN values& body(); ///< Allows in-place modification of body sections.

    // FIXME aconway 2015-06-17: consistent and flexible treatment of buffers.
    // Allow convenient std::string encoding/decoding (with re-use of existing
    // string capacity) but also need to allow encoding/decoding of non-string
    // buffers. Introduce a buffer type with begin/end pointers?

    PN_CPP_EXTERN void encode(std::string &data);
    PN_CPP_EXTERN std::string encode();
    PN_CPP_EXTERN void decode(const std::string &data);

  private:
    mutable values body_;
  friend class proton_impl_ref<message>;
};

}

#endif  /*!PROTON_CPP_MESSAGE_H*/
