#ifndef PROTON_URL_HPP
#define PROTON_URL_HPP

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

#include "./internal/pn_unique_ptr.hpp"
#include "./error.hpp"

#include <proton/type_compat.h>

#include <iosfwd>
#include <string>

/// @file
/// **Deprecated** - Use a third-party URL library.

namespace proton {

/// **Deprecated** - Use a third-party URL library.
///
/// An error encountered during URL parsing.

struct
PN_CPP_DEPRECATED("Use a third-party URL library")
PN_CPP_CLASS_EXTERN url_error : public error {
    /// @cond INTERNAL
    /// Construct a URL error with a message.
    PN_CPP_EXTERN explicit url_error(const std::string&);
    /// @endcond
};

/// **Deprecated** - Use a third-party URL library.
///
/// A URL parser.
///
///  Proton URLs take the form
/// `<scheme>://<username>:<password>@<host>:<port>/<path>`.
///
/// - Scheme can be `amqp` or `amqps`.  Host is a DNS name or IP
///   address (v4 or v6).
///
/// - Port can be a number or a symbolic service name such as `amqp`.
///
/// - Path is normally used as a link source or target address.  On a
///   broker it typically corresponds to a queue or topic name.
class PN_CPP_DEPRECATED("Use a third-party URL library") url {
  public:
    static const std::string AMQP;     ///< "amqp" prefix
    static const std::string AMQPS;    ///< "amqps" prefix

    // XXX No constructor for an empty URL?
    // XXX What is the default 'defaults' behavior?

    /// Parse `url_str` as an AMQP URL.
    ///
    /// @note Converts automatically from string.
    /// @throw url_error if URL is invalid.
    PN_CPP_EXTERN url(const std::string& url_str);

    /// @cond INTERNAL
    /// XXX I want to understand why this is important to keep.
    ///
    /// **Unsettled API** - Parse `url_str` as an AMQP URL. If
    /// `defaults` is true, fill in defaults for missing values.
    /// Otherwise, return an empty string for missing values.
    ///
    /// @note Converts automatically from string.
    /// @throw url_error if URL is invalid.
    PN_CPP_EXTERN url(const std::string& url_str, bool defaults);
    /// @endcond

    /// Copy a URL.
    PN_CPP_EXTERN url(const url&);

    PN_CPP_EXTERN ~url();

    /// Copy a URL.
    PN_CPP_EXTERN url& operator=(const url&);

    /// True if the URL is empty.
    PN_CPP_EXTERN bool empty() const;

    /// Returns the URL as a string
    PN_CPP_EXTERN operator std::string() const;

    /// @name URL fields
    ///
    /// @{

    /// `amqp` or `amqps`.
    PN_CPP_EXTERN std::string scheme() const;
    /// The user name for authentication.
    PN_CPP_EXTERN std::string user() const;
    // XXX Passwords in URLs are dumb.
    /// The password.
    PN_CPP_EXTERN std::string password() const;
    /// The host name or IP address.
    PN_CPP_EXTERN std::string host() const;
    /// `port` can be a number or a symbolic name such as "amqp".
    PN_CPP_EXTERN std::string port() const;
    /// `port_int` is the numeric value of the port.
    PN_CPP_EXTERN uint16_t port_int() const;
    /// host_port returns just the `host:port` part of the URL
    PN_CPP_EXTERN std::string host_port() const;

    // XXX is this not confusing (or incorrect)?  The path starts with
    // the first / after //.
    /// `path` is everything after the final "/".
    PN_CPP_EXTERN std::string path() const;

    /// @}

    /// Return URL as a string.
    friend PN_CPP_EXTERN std::string to_string(const url&);

  private:
    struct impl;
    internal::pn_unique_ptr<impl> impl_;

    /// @cond INTERNAL

  friend PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const url&);

    // XXX Why is it important to have this?
    /// Parse `url` from istream.  This automatically fills in
    /// defaults for missing values.
    ///
    /// @note An invalid url is indicated by setting
    /// std::stream::fail(), NOT by throwing url_error.
  friend PN_CPP_EXTERN std::istream& operator>>(std::istream&, url&);

    /// @endcond
};

} // proton

#endif // PROTON_URL_HPP
