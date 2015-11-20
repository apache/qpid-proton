#ifndef MESSAGE_ID_HPP
#define MESSAGE_ID_HPP
/*
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
 */

#include "proton/types.hpp"
#include "proton/value.hpp"

namespace proton {
/** A message_id can contain one of the following types:
 * uint64_t (aka amqp_ulong), amqp_uuid, amqp_binary or amqp_string.
 */
class message_id : public comparable<message_id> {
  public:
    message_id() {}
    message_id(const uint64_t& x) : value_(x) {}
    message_id(const amqp_uuid& x) : value_(x) {}
    message_id(const amqp_binary& x) : value_(x) {}
    message_id(const amqp_string& x) : value_(x) {}
    /// string is encoded as amqp_string
    message_id(const std::string& x) : value_(x) {}
    message_id(const char *x) : value_(x) {}

    message_id& operator=(const message_id& x) { value_ = x.value_; return *this; }
    message_id& operator=(const uint64_t& x) { value_ = x; return *this; }
    message_id& operator=(const amqp_uuid& x) { value_ = x; return *this; }
    message_id& operator=(const amqp_binary& x) { value_ = x; return *this; }
    message_id& operator=(const amqp_string& x) { value_ = x; return *this; }
    /// string is encoded as amqp_string
    message_id& operator=(const std::string& x) { value_ = x; return *this; }
    message_id& operator=(const char *x) { value_ = x; return *this; }

    void clear() { value_.clear(); }
    bool empty() const { return value_.empty(); }
    type_id type() { return value_.type(); }

    void get(uint64_t& x) const { value_.get(x); }
    void get(amqp_uuid& x) const { value_.get(x); }
    void get(amqp_binary& x) const { value_.get(x); }
    void get(amqp_string& x) const { value_.get(x); }
    /// Both amqp_binary and amqp_string can be converted to std::string
    void get(std::string& x) const { value_.get(x); }

    template<class T> T get() const { T x; get(x); return x; }

    // String representation: decimal representation of uint64_t, standard text
    // representation of UUID, amqp_string or amqp_binary are returned without
    // modification.
    std::string& str() const;

    bool operator==(const message_id& x) const { return value_ == x.value_; }
    bool operator<(const message_id& x) const { return value_ < x.value_; }

  friend std::ostream& operator<<(std::ostream&, const message_id&);
  friend PN_CPP_EXTERN encoder operator<<(encoder, const message_id&);
  friend PN_CPP_EXTERN decoder operator>>(decoder, message_id&);

  private:
    //message_id(const data d) : value_(d) {}
    value value_;
  friend class message;
  friend class data;
};

}
#endif // MESSAGE_ID_HPP
