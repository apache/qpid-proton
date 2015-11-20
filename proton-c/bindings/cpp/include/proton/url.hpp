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

#include "proton/types.hpp"
#include "proton/error.hpp"
#include <iosfwd>

struct pn_url_t;

namespace proton {

/// Thrown if URL parsing fails.
struct url_error : public error {
    PN_CPP_EXTERN explicit url_error(const std::string&) throw();
};


/**
 * url is a proton URL of the form `<scheme>://<username>:<password>@<host>:<port>/<path>`.
 * scheme can be `amqp` or `amqps`. host is a DNS name or IP address (v4 or v6)
 * port can be a number or symbolic service name like `amqp`. path is normally used as
 * a link source or target address, on a broker it typically it corresponds to a queue or topic name.
 */
class url {
  public:
    static const std::string AMQP;     ///< "amqp" prefix
    static const std::string AMQPS;    ///< "amqps" prefix

    /** Create an empty url */
    PN_CPP_EXTERN url();

    /** Parse url_str as an AMQP URL. If defaults is true, fill in defaults for missing values
     *  otherwise return an empty string for missing values.
     *  Note: converts automatically from string.
     *@throw url_error if URL is invalid.
     */
    PN_CPP_EXTERN url(const std::string& url_str, bool defaults=true);

    /** Parse url_str as an AMQP URL. If defaults is true, fill in defaults for missing values
     *  otherwise return an empty string for missing values.
     *  Note: converts automatically from string.
     *@throw url_error if URL is invalid.
     */
    PN_CPP_EXTERN url(const char* url_str, bool defaults=true);

    PN_CPP_EXTERN url(const url&);
    PN_CPP_EXTERN ~url();
    PN_CPP_EXTERN url& operator=(const url&);

    /** Parse a string as a URL
     *@throws url_error if URL is invalid.
     */
    PN_CPP_EXTERN void parse(const std::string&);

    /** Parse a string as a URL
     *@throws url_error if URL is invalid.
     */
    PN_CPP_EXTERN void parse(const char*);

    PN_CPP_EXTERN bool empty() const;

    /** str returns the URL as a string string */
    PN_CPP_EXTERN std::string str() const;

    /**@name Get parts of the URL
     *@{
     */
    PN_CPP_EXTERN std::string scheme() const;
    PN_CPP_EXTERN std::string username() const;
    PN_CPP_EXTERN std::string password() const;
    PN_CPP_EXTERN std::string host() const;
    /** port is a string, it can be a number or a symbolic name like "amqp" */
    PN_CPP_EXTERN std::string port() const;
    /** port_int is the numeric value of the port. */
    PN_CPP_EXTERN uint16_t port_int() const;
    /** path is everything after the final "/" */
    PN_CPP_EXTERN std::string path() const;
    //@}

    /** host_port returns just the host:port part of the URL */
    PN_CPP_EXTERN std::string host_port() const;

    /**@name Set parts of the URL
     *@{
     */
    PN_CPP_EXTERN void scheme(const std::string&);
    PN_CPP_EXTERN void username(const std::string&);
    PN_CPP_EXTERN void password(const std::string&);
    PN_CPP_EXTERN void host(const std::string&);
    /** port is a string, it can be a number or a symbolic name like "amqp" */
    PN_CPP_EXTERN void port(const std::string&);
    PN_CPP_EXTERN void path(const std::string&);
    //@}

    /** defaults fills in default values for missing parts of the URL */
    PN_CPP_EXTERN void defaults();

  friend PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const url&);

    /** parse url from istream, automatically fills in defaults for missing values.
     *
     * Note: an invalid url is indicated by setting std::stream::fail() NOT by throwing url_error.
     */
  friend PN_CPP_EXTERN std::istream& operator>>(std::istream&, url&);

  private:
    pn_url_t* url_;
};


}

#endif // URL_HPP
