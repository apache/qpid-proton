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

#include "proton_bits.hpp"
#include "proton/error_condition.hpp"

#include <string>
#include <ostream>

#include <proton/condition.h>
#include <proton/error.h>
#include <proton/object.h>

namespace proton {

std::string error_str(long code) {
  switch (code)
  {
  case 0: return "ok";
  case PN_EOS: return "end of data stream";
  case PN_ERR: return "error";
  case PN_OVERFLOW: return "overflow";
  case PN_UNDERFLOW: return "underflow";
  case PN_STATE_ERR: return "invalid state";
  case PN_ARG_ERR: return "invalid argument";
  case PN_TIMEOUT: return "timeout";
  case PN_INTR: return "interrupt";
  default: return "unknown error code";
  }
}

std::string error_str(pn_error_t* err, long code) {
    if (err && pn_error_code(err)) {
        const char* text = pn_error_text(err);
        return text ? std::string(text) : error_str(pn_error_code(err));
    }
    return error_str(code);
}

std::ostream& operator<<(std::ostream& o, const inspectable& object) {
    pn_string_t* str = pn_string("");
    pn_inspect(object.value, str);
    o << pn_string_get(str);
    pn_free(str);
    return o;
}

void set_error_condition(const error_condition& e, pn_condition_t *c) {
    pn_condition_clear(c);

    if (!e.name().empty()) {
        pn_condition_set_name(c, e.name().c_str());
    }
    if (!e.description().empty()) {
        pn_condition_set_description(c, e.description().c_str());
    }
    value(pn_condition_info(c)) = e.properties();
}

}
