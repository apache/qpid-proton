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
#include "proton/condition.h"
namespace {
inline std::string safe_convert(const char* s) {
    return s ? s : std::string();
}
}

namespace proton {

error_condition::error_condition(pn_condition_t* c) :
    name_(safe_convert(pn_condition_get_name(c))),
    description_(safe_convert(pn_condition_get_description(c))),
    properties_(pn_condition_info(c))
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

}
