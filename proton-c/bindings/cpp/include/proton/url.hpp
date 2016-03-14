#ifndef URL_HPP
#define URL_HPP

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

#include <proton/types_fwd.hpp>
#include <proton/error.hpp>

#include <iosfwd>

struct pn_url_t;

namespace proton {

/// Raised if URL parsing fails.
struct
PN_CPP_CLASS_EXTERN url_error : public error {
    /// @cond INTERNAL
    PN_CPP_EXTERN explicit url_error(const std::string&); ///< Construct with message
    /// @endcond
};

/// A proton URL.
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
class url {
  public:
    static const std::string AMQP;     ///< "amqp" prefix
    static const std::string AMQPS;    ///< "amqps" prefix

    /// Create an empty URL
    PN_CPP_EXTERN url();

    /// Parse `url_str` as an AMQP URL. If defaults is true, fill in
    /// defaults for missing values otherwise return an empty string
    /// for missing values.
    ///
    /// @note Converts automatically from string.
    ///
    /// @throw url_error if URL is invalid.
    PN_CPP_EXTERN url(const std::string& url_str, bool defaults=true);

    /// Parse `url_str` as an AMQP URL. If defaults is true, fill in
    /// defaults for missing values otherwise return an empty string
    /// for missing values.
    ///
    /// @note Converts automatically from string.
    ///
    /// @throw url_error if URL is invalid.
    PN_CPP_EXTERN url(const char* url_str, bool defaults=true);

    /// Copy a URL.
    PN_CPP_EXTERN url(const url&);
    PN_CPP_EXTERN ~url();
    /// Copy a URL.
    PN_CPP_EXTERN url& operator=(const url&);

    /// Parse a string as a URL.
    ///
    /// @throws url_error if URL is invalid.
    PN_CPP_EXTERN void parse(const std::string&);

    /// Parse a string as a URL.
    ///
    /// @throws url_error if URL is invalid.
    PN_CPP_EXTERN void parse(const char*);

    /// True if the URL is empty.
    PN_CPP_EXTERN bool empty() const;

    /// `str` returns the URL as a string
    PN_CPP_EXTERN std::string str() const;

    /// @name URL fields
    ///
    /// @{

    PN_CPP_EXTERN std::string scheme() const;
    PN_CPP_EXTERN void scheme(const std::string&);

    /// @cond INTERNAL
    PN_CPP_EXTERN std::string username() const;
    PN_CPP_EXTERN void username(const std::string&);
    /// @endcond

    PN_CPP_EXTERN std::string password() const;
    PN_CPP_EXTERN void password(const std::string&);

    PN_CPP_EXTERN std::string host() const;
    PN_CPP_EXTERN void host(const std::string&);
    /// `port` can be a number or a symbolic name such as "amqp".
    PN_CPP_EXTERN void port(const std::string&);
    PN_CPP_EXTERN std::string port() const;
    /// `port_int` is the numeric value of the port.
    PN_CPP_EXTERN uint16_t port_int() const;
    /// host_port returns just the `host:port` part of the URL
    PN_CPP_EXTERN std::string host_port() const;

    /// `path` is everything after the final "/".
    PN_CPP_EXTERN std::string path() const;
    PN_CPP_EXTERN void path(const std::string&);

    /// @}

    /// @cond INTERNAL
    /// XXX need to discuss
    /// defaults fills in default values for missing parts of the URL.
    PN_CPP_EXTERN void defaults();
    /// @endcond

    /// @cond INTERNAL
  private:
    pn_url_t* url_;

    friend PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const url&);

    /// Parse `url` from istream.  This automatically fills in
    /// defaults for missing values.
    ///
    /// @note An invalid url is indicated by setting
    /// std::stream::fail(), NOT by throwing url_error.
    friend PN_CPP_EXTERN std::istream& operator>>(std::istream&, url&);

    /// @endcond
};

}

#endif // URL_HPP
