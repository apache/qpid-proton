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
#include "proton/ProtonHandle.hpp"
#include "proton/Value.hpp"
#include "proton/Message.hpp"
#include <string>

struct pn_message_t;
struct pn_data_t;

namespace proton {

// FIXME aconway 2015-06-17: documentation of properties.
class Message : public reactor::ProtonHandle<pn_message_t>
{
  public:
    PN_CPP_EXTERN Message();
    PN_CPP_EXTERN Message(pn_message_t *);
    PN_CPP_EXTERN Message(const Message&);
    PN_CPP_EXTERN Message& operator=(const Message&);
    PN_CPP_EXTERN ~Message();

    PN_CPP_EXTERN pn_message_t *pnMessage() const;

    PN_CPP_EXTERN void id(const Value& id);
    PN_CPP_EXTERN Value id() const;

    PN_CPP_EXTERN void user(const std::string &user);
    PN_CPP_EXTERN std::string user() const;

    PN_CPP_EXTERN void address(const std::string &addr);
    PN_CPP_EXTERN std::string address() const;

    PN_CPP_EXTERN void subject(const std::string &s);
    PN_CPP_EXTERN std::string subject() const;

    PN_CPP_EXTERN void replyTo(const std::string &s);
    PN_CPP_EXTERN std::string replyTo() const;

    PN_CPP_EXTERN void correlationId(const Value&);
    PN_CPP_EXTERN Value correlationId() const;

    PN_CPP_EXTERN void contentType(const std::string &s);
    PN_CPP_EXTERN std::string contentType() const;

    PN_CPP_EXTERN void contentEncoding(const std::string &s);
    PN_CPP_EXTERN std::string contentEncoding() const;

    PN_CPP_EXTERN void expiry(Timestamp t);
    PN_CPP_EXTERN Timestamp expiry() const;

    PN_CPP_EXTERN void creationTime(Timestamp t);
    PN_CPP_EXTERN Timestamp creationTime() const;

    PN_CPP_EXTERN void groupId(const std::string &s);
    PN_CPP_EXTERN std::string groupId() const;

    PN_CPP_EXTERN void replyToGroupId(const std::string &s);
    PN_CPP_EXTERN std::string replyToGroupId() const;

    /** Set the body to an AMQP value. */
    PN_CPP_EXTERN void body(const Value&);

    /** Template to convert any type to a Value and set as the body */
    template <class T> void body(const T& v) { body(Value(v)); }

    /** Set the body to a sequence of sections containing AMQP values. */
    PN_CPP_EXTERN void body(const Values&);

    PN_CPP_EXTERN const Values& body() const;

    PN_CPP_EXTERN Values& body(); ///< Allows in-place modification of body sections.

    // FIXME aconway 2015-06-17: consistent and flexible treatment of buffers.
    // Allow convenient std::string encoding/decoding (with re-use of existing
    // string capacity) but also need to allow encoding/decoding of non-string
    // buffers. Introduce a buffer type with begin/end pointers?

    PN_CPP_EXTERN void encode(std::string &data);
    PN_CPP_EXTERN std::string encode();
    PN_CPP_EXTERN void decode(const std::string &data);

  private:
    mutable Values body_;
  friend class reactor::ProtonImplRef<Message>;
};

}

#endif  /*!PROTON_CPP_MESSAGE_H*/
