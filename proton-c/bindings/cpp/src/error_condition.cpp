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
#include "proton/error_condition.hpp"

#include <proton/condition.h>

#include "proton_bits.hpp"

#include <ostream>

namespace proton {

error_condition::error_condition(pn_condition_t* c) :
    name_(str(pn_condition_get_name(c))),
    description_(str(pn_condition_get_description(c))),
    properties_(value(pn_condition_info(c)))
{}


error_condition::error_condition(std::string description) :
    name_("proton:io:error"),
    description_(description)
{}

error_condition::error_condition(std::string name, std::string description) :
    name_(name),
    description_(description)
{}

error_condition::error_condition(std::string name, std::string description, value properties) :
    name_(name),
    description_(description),
    properties_(properties)
{}

#if PN_CPP_HAS_EXPLICIT_CONVERSIONS
error_condition::operator bool() const {
  return !name_.empty();
}
#endif

bool error_condition::operator!() const {
    return name_.empty();
}

bool error_condition::empty() const {
    return name_.empty();
}

std::string error_condition::name() const {
    return name_;
}

std::string error_condition::description() const {
    return description_;
}

value error_condition::properties() const {
    return properties_;
}

std::string error_condition::what() const {
    if (!*this) {
        return "No error condition";
    } else {
      std::string s(name_);
      if (!description_.empty()) {
          s += ": ";
          s += description_;
      }
      return s;
    }
}

bool operator==(const error_condition& x, const error_condition& y) {
    return x.name() == y.name() && x.description() == y.description()
        && x.properties() == y.properties();
}

std::ostream& operator<<(std::ostream& o, const error_condition& err) {
    return o << err.what();
}

}
