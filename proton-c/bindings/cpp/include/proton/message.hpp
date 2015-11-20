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
#include "proton/message_id.hpp"
#include "proton/pn_unique_ptr.hpp"
#include "proton/value.hpp"

#include <string>
#include <utility>

struct pn_message_t;

namespace proton {

class link;
class delivery;
class message_id;

/** An AMQP message. Value semantics, can be copied or assigned to make a new message. */
class message
{
  public:
    PN_CPP_EXTERN message();
    PN_CPP_EXTERN message(const message&);
    PN_CPP_EXTERN message(const value&);

#if PN_HAS_CPP11
    PN_CPP_EXTERN message(message&&);
#endif
    PN_CPP_EXTERN ~message();
    PN_CPP_EXTERN message& operator=(const message&);

    void swap(message& x);

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

    /** Set the body. */
    PN_CPP_EXTERN void body(const value&);

    /** Get the body. Note data can be copied to a proton::value */
    PN_CPP_EXTERN const data body() const;

    /** Get a reference to the body data, can be modified in-place. */
    PN_CPP_EXTERN data body();

    /** Set the application properties. Must be a map with string keys or an
     * empty value. You can assign to a proton::value from a standard C++ map
     * of std::string to proton::value.
     */
    PN_CPP_EXTERN void properties(const value&);

    /** Get the application properties, which will be a map with string keys or
     * an empty value. You can assign proton::value containing a map to a
     * standard C++ map of std::string to proton::value.
     */
    PN_CPP_EXTERN const data properties() const;

    /** Get a reference to the application properties, can be modified in-place.*/
    PN_CPP_EXTERN data properties();

    /** Set an individual application property. */
    PN_CPP_EXTERN void property(const std::string &name, const value &);

    /** Get an individual application property. Returns an empty value if not found. */
    PN_CPP_EXTERN value property(const std::string &name) const;

    /** Erase an application property. Returns false if there was no such property. */
    PN_CPP_EXTERN bool erase_property(const std::string &name);

    /** Encode into a string, growing the string if necessary. */
    PN_CPP_EXTERN void encode(std::string &bytes) const;

    /** Return encoded message as a string */
    PN_CPP_EXTERN std::string encode() const;

    /** Decode from string data into the message. */
    PN_CPP_EXTERN void decode(const std::string &bytes);

    /// Decode the message from link corresponding to delivery.
    PN_CPP_EXTERN void decode(proton::link, proton::delivery);

    /**
     * Get the inferred flag for a message.
     *
     * The inferred flag for a message indicates how the message content
     * is encoded into AMQP sections. If inferred is true then binary and
     * list values in the body of the message will be encoded as AMQP DATA
     * and AMQP SEQUENCE sections, respectively. If inferred is false,
     * then all values in the body of the message will be encoded as AMQP
     * VALUE sections regardless of their type. Use
     * ::pn_message_set_inferred to set the value.
     *
     * @param[in] msg a message object
     * @return the value of the inferred flag for the message
     */
    PN_CPP_EXTERN bool inferred() const;
    PN_CPP_EXTERN void inferred(bool);

  private:
    pn_message_t *message_;
};

}

#endif  /*!PROTON_CPP_MESSAGE_H*/
