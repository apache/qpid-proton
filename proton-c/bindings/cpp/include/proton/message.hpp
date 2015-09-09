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

struct pn_message_t;

namespace proton {

class link;
class delivery;

/** An AMQP message. Not directly construct-able, use create() or message_value.*/
class message : public facade<pn_message_t, message>
{
  public:
    PN_CPP_EXTERN static pn_unique_ptr<message> create();

    /// Copy data from m to this.
    PN_CPP_EXTERN message& operator=(const message& m);

    /** Clear the message content and properties. */
    PN_CPP_EXTERN void clear();

    ///@name Message properties
    ///@{

    /// Globally unique identifier, can be an a string, an unsigned long, a uuid or a binary value.
    PN_CPP_EXTERN void id(const data& id);
    /// Globally unique identifier, can be an a string, an unsigned long, a uuid or a binary value.
    PN_CPP_EXTERN const data& id() const;
    PN_CPP_EXTERN data& id();

    PN_CPP_EXTERN void user(const std::string &user);
    PN_CPP_EXTERN std::string user() const;

    PN_CPP_EXTERN void address(const std::string &addr);
    PN_CPP_EXTERN std::string address() const;

    PN_CPP_EXTERN void subject(const std::string &s);
    PN_CPP_EXTERN std::string subject() const;

    PN_CPP_EXTERN void reply_to(const std::string &s);
    PN_CPP_EXTERN std::string reply_to() const;

    /// Correlation identifier, can be an a string, an unsigned long, a uuid or a binary value.
    PN_CPP_EXTERN void correlation_id(const data&);
    /// Correlation identifier, can be an a string, an unsigned long, a uuid or a binary value.
    PN_CPP_EXTERN const data& correlation_id() const;
    PN_CPP_EXTERN data& correlation_id();

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

    /** Set the body. If data has more than one value, each is encoded as an AMQP section. */
    PN_CPP_EXTERN void body(const data&);

    /** Set the body to any type T that can be converted to proton::data */
    template <class T> void body(const T& v) { body().clear(); body().encoder() << v; }

    /** Get the body values. */
    PN_CPP_EXTERN const data& body() const;

    /** Get a reference to the body data, can be modified in-place. */
    PN_CPP_EXTERN data& body();

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
    PN_CPP_EXTERN void decode(proton::link&, proton::delivery&);

    PN_CPP_EXTERN void operator delete(void*);
};


/** A message with value semantics */
class message_value {
  public:
    message_value() : message_(message::create()) {}
    message_value(const message_value& x) : message_(message::create()) { *message_ = *x.message_; }
    message_value(const message& x) : message_(message::create()) { *message_ = x; }
    message_value& operator=(const message_value& x) { *message_ = *x.message_; return *this; }
    message_value& operator=(const message& x) { *message_ = x; return *this; }

    // TODO aconway 2015-09-02: C++11 move semantics.

    operator message&() { return *message_; }
    operator const message&() const { return *message_; }

    /** Clear the message content */
    void clear() { message_->clear(); }

    ///@name Message properties
    ///@{

    /// Globally unique identifier, can be an a string, an unsigned long, a uuid or a binary value.
    void id(const data& id) { message_->id(id); }
    /// Globally unique identifier, can be an a string, an unsigned long, a uuid or a binary value.
    const data& id() const { return message_->id(); }
    data& id() { return message_->id(); }

    void user(const std::string &user) { message_->user(user); }
    std::string user() const { return message_->user(); }

    void address(const std::string &addr) { message_->address(addr); }
    std::string address() const { return message_->address(); }

    void subject(const std::string &s) { message_->subject(s); }
    std::string subject() const { return message_->subject(); }

    void reply_to(const std::string &s) { message_->reply_to(s); }
    std::string reply_to() const { return message_->reply_to(); }

    /// Correlation identifier, can be an a string, an unsigned long, a uuid or a binary value.
    void correlation_id(const data& d) { message_->correlation_id(d); }
    /// Correlation identifier, can be an a string, an unsigned long, a uuid or a binary value.
    const data& correlation_id() const { return message_->correlation_id(); }
    data& correlation_id() { return message_->correlation_id(); }

    void content_type(const std::string &s) { message_->content_type(s); }
    std::string content_type() const { return message_->content_type(); }

    void content_encoding(const std::string &s) { message_->content_encoding(s); }
    std::string content_encoding() const { return message_->content_encoding(); }

    void expiry(amqp_timestamp t) { message_->expiry(t); }
    amqp_timestamp expiry() const { return message_->expiry(); }

    void creation_time(amqp_timestamp t) { message_->creation_time(t); }
    amqp_timestamp creation_time() const { return message_->creation_time(); }

    void group_id(const std::string &s) { message_->group_id(s); }
    std::string group_id() const { return message_->group_id(); }

    void reply_to_group_id(const std::string &s) { message_->reply_to_group_id(s); }
    std::string reply_to_group_id() const { return message_->reply_to_group_id(); }
    ///@}

    /** Set the body. If data has more than one value, each is encoded as an AMQP section. */
    void body(const data& d) { message_->body(d); }

    /** Set the body to any type T that can be converted to proton::data */
    template <class T> void body(const T& v) { message_->body(v); }

    /** Get the body values. */
    const data& body() const { return message_->body(); }

    /** Get a reference to the body data, can be modified in-place. */
    data& body() { return message_->body(); }

    // TODO aconway 2015-06-17: consistent and flexible treatment of buffers.
    // Allow convenient std::string encoding/decoding (with re-use of existing
    // string capacity) but also need to allow encoding/decoding of non-string
    // buffers. Introduce a buffer type with begin/end pointers?

    /** Encode the message into string data */
    void encode(std::string &data) const { message_->encode(data); }
    /** Retrun encoded message as a string */
    std::string encode() const { return message_->encode(); }
    /** Decode from string data into the message. */
    void decode(const std::string &data) { message_->decode(data); }

    /// Decode the message from link corresponding to delivery.
    void decode(proton::link& l, proton::delivery& d) { message_->decode(l, d); }

    void swap(message_value& x);

  private:
    pn_unique_ptr<class message> message_;
};

}

#endif  /*!PROTON_CPP_MESSAGE_H*/
