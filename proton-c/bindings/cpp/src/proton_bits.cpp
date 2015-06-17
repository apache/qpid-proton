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

#include <string>
#include <ostream>
#include <proton/error.h>
#include <proton/object.h>
#include "proton_bits.hpp"

std::string errorStr(int code) {
  switch (code)
  {
  case 0: return "ok";
  case PN_EOS: return "end of data stream";
  case PN_ERR: return "error";
  case PN_OVERFLOW: return "overflow";
  case PN_UNDERFLOW: return "underflow";
  case PN_STATE_ERR: return "invalid state";
  case PN_ARG_ERR: return "invalud argument";
  case PN_TIMEOUT: return "timeout";
  case PN_INTR: return "interrupt";
  default: return "unknown error code";
  }
}

std::string errorStr(pn_error_t* err, int code) {
    if (err && pn_error_code(err)) {
        const char* text = pn_error_text(err);
        return text ? std::string(text) : errorStr(pn_error_code(err));
    }
    return errorStr(code);
}

std::ostream& operator<<(std::ostream& o, const Object& object) {
    pn_string_t* str = pn_string("");
    pn_inspect(object.value, str);
    o << pn_string_get(str);
    pn_free(str);
    return o;
}
