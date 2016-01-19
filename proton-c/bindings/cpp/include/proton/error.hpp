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
#include "proton/config.hpp"
#include "proton/export.hpp"

#include <stdexcept>
#include <string>

namespace proton {

/** Functions in the proton namespace throw a subclass of proton::error on error. */
struct error : public std::runtime_error { PN_CPP_EXTERN explicit error(const std::string&); };

/** Raised if timeout expires */
struct timeout_error : public error { PN_CPP_EXTERN explicit timeout_error(const std::string&); };

/** Raised if there is an error decoding AMQP data as a C++ value. */
struct decode_error : public error { PN_CPP_EXTERN explicit decode_error(const std::string&); };

/** Raised if there is an error encoding a C++ value as AMQP data. */
struct encode_error : public error { PN_CPP_EXTERN explicit encode_error(const std::string&); };

/** Error reading or writing external IO. */
struct io_error : public error { PN_CPP_EXTERN explicit io_error(const std::string&); };

/** Attempt to use a closed resource (connnection, session or link). */
struct closed_error : public io_error {
    PN_CPP_EXTERN explicit closed_error(const std::string& = default_msg);
    static const std::string default_msg;
};

}
#endif  /*!PROTON_CPP_EXCEPTIONS_H*/
