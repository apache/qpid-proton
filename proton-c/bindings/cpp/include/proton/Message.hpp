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
#include "proton/ImportExport.hpp"
#include "proton/ProtonHandle.hpp"
#include "proton/Value.hpp"
#include "proton/Message.hpp"
#include <string>

struct pn_message_t;
struct pn_data_t;

namespace proton {

// FIXME aconway 2015-06-17: documentation of properties.
PN_CPP_EXTERN class Message : public reactor::ProtonHandle<pn_message_t>
{
  public:
    Message();
    Message(pn_message_t *);
    Message(const Message&);
    Message& operator=(const Message&);
    ~Message();

    pn_message_t *pnMessage() const;

    void id(const Value& id);
    Value id() const;

    void user(const std::string &user);
    std::string user() const;

    void address(const std::string &addr);
    std::string address() const;

    void subject(const std::string &s);
    std::string subject() const;

    void replyTo(const std::string &s);
    std::string replyTo() const;

    void correlationId(const Value&);
    Value correlationId() const;

    void contentType(const std::string &s);
    std::string contentType() const;

    void contentEncoding(const std::string &s);
    std::string contentEncoding() const;

    void expiry(Timestamp t);
    Timestamp expiry() const;

    void creationTime(Timestamp t);
    Timestamp creationTime() const;

    void groupId(const std::string &s);
    std::string groupId() const;

    void replyToGroupId(const std::string &s);
    std::string replyToGroupId() const;

    /** Set the body to an AMQP value. */
    void body(const Value&);

    /** Template to convert any type to a Value and set as the body */
    template <class T> void body(const T& v) { body(Value(v)); }

    /** Set the body to a sequence of sections containing AMQP values. */
    void body(const Values&);

    const Values& body() const;

    Values& body(); ///< Allows in-place modification of body sections.

    // FIXME aconway 2015-06-17: consistent and flexible treatment of buffers.
    // Allow convenient std::string encoding/decoding (with re-use of existing
    // string capacity) but also need to allow encoding/decoding of non-string
    // buffers. Introduce a buffer type with begin/end pointers?

    void encode(std::string &data);
    std::string encode();
    void decode(const std::string &data);

  private:
    mutable Values body_;
  friend class reactor::ProtonImplRef<Message>;
};

}

#endif  /*!PROTON_CPP_MESSAGE_H*/
