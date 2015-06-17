#ifndef PROTON_CPP_EXCEPTIONS_H
#define PROTON_CPP_EXCEPTIONS_H

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
#include <stdexcept>
#include <string>
#include "proton/export.hpp"

namespace proton {

/** @ingroup cpp
 * Functions in the proton namespace throw a subclass of proton::Error on error.
 */
struct Error : public std::runtime_error { PN_CPP_EXTERN explicit Error(const std::string&) throw(); };

/** Raised if a message is rejected */
struct MessageReject : public Error { PN_CPP_EXTERN explicit MessageReject(const std::string&) throw(); };

/** Raised if a message is released */
struct MessageRelease : public Error { PN_CPP_EXTERN explicit MessageRelease(const std::string&) throw(); };


}

#endif  /*!PROTON_CPP_EXCEPTIONS_H*/
