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

#include "proton/url.hpp"

#include "proton/error.hpp"

#include "proton_bits.hpp"

#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <vector>

namespace {

/** URL-encode src and append to dst. */
static std::string pni_urlencode(const std::string &src) {
    static const char *bad = "@:/%[]?#";

    std::ostringstream dst;
    dst << std::hex << std::uppercase << std::setfill('0');

    std::size_t i = 0;
    std::size_t j = src.find_first_of(bad);
    while (j!=std::string::npos) {
        dst << src.substr(i, j-i);
        dst << "%" << std::setw(2) << int(src[j]);
        i = j + 1;
        j = src.find_first_of(bad, i);
    }
    dst << src.substr(i);
    return dst.str();
}

// Low level url parser
static void pni_urldecode(const char *src, char *dst)
{
  const char *in = src;
  char *out = dst;
  while (*in != '\0')
  {
    if ('%' == *in)
    {
      if ((in[1] != '\0') && (in[2] != '\0'))
      {
        char esc[3];
        esc[0] = in[1];
        esc[1] = in[2];
        esc[2] = '\0';
        unsigned long d = std::strtoul(esc, NULL, 16);
        *out = (char)d;
        in += 3;
        out++;
      }
      else
      {
        *out = *in;
        in++;
        out++;
      }
    }
    else
    {
      *out = *in;
      in++;
      out++;
    }
  }
  *out = '\0';
}

void parse_url(char *url, const char **scheme, const char **user, const char **pass, const char **host, const char **port, const char **path)
{
  if (!url) return;

  char *slash = std::strchr(url, '/');

  if (slash && slash>url) {
    char *scheme_end = std::strstr(slash-1, "://");

    if (scheme_end && scheme_end<slash) {
      *scheme_end = '\0';
      *scheme = url;
      url = scheme_end + 3;
      slash = std::strchr(url, '/');
    }
  } else if (0 == strncmp(url, "//", 2)) {
      url += 2;
      slash = std::strchr(url, '/');
  }

  if (slash) {
    *slash = '\0';
    *path = slash + 1;
  }

  char *at = std::strchr(url, '@');
  if (at) {
    *at = '\0';
    char *up = url;
    url = at + 1;
    char *colon = std::strchr(up, ':');
    if (colon) {
      *colon = '\0';
      char *p = colon + 1;
      pni_urldecode(p, p);
      *pass = p;
    }
    pni_urldecode(up, up);
    *user = up;
  }

  *host = url;
  char *open = (*url == '[') ? url : 0;
  if (open) {
    char *close = std::strchr(open, ']');
    if (close) {
        *host = open + 1;
        *close = '\0';
        url = close + 1;
    }
  }

  char *colon = std::strchr(url, ':');
  if (colon) {
    *colon = '\0';
    *port = colon + 1;
  }
}

} // namespace

namespace proton {

struct url::impl {
    static const char* const default_host;
    const char* scheme;
    const char* username;
    const char* password;
    const char* host;
    const char* port;
    const char* path;
    std::vector<char> cstr;
    mutable std::string str;

    impl(const std::string& s) :
        scheme(0), username(0), password(0), host(0), port(0), path(0),
        cstr(s.size()+1, '\0')
    {
        std::copy(s.begin(), s.end(), cstr.begin());
        parse_url(&cstr[0], &scheme, &username, &password, &host, &port, &path);
    }

    void defaults() {
        if (!scheme || *scheme=='\0' ) scheme = proton::url::AMQP.c_str();
        if (!host || *host=='\0' ) host = default_host;
        if (!port || *port=='\0' ) port = scheme;
    }

    operator std::string() const {
        if ( str.empty() ) {
            if (scheme) {
                str += scheme;
                str += "://";
            }
            if (username) {
                str += pni_urlencode(username);
            }
            if (password) {
                str += ":";
                str += pni_urlencode(password);
            }
            if (username || password) {
                str += "@";
            }
            if (host) {
                if (std::strchr(host, ':')) {
                    str += '[';
                    str += host;
                    str += ']';
                } else {
                    str += host;
                }
            }
            if (port) {
                str += ':';
                str += port;
            }
            if (path) {
                str += '/';
                str += path;
            }
        }
        return str;
    }

};

const char* const url::impl::default_host = "localhost";


url_error::url_error(const std::string& s) : error(s) {}

url::url(const std::string &s) : impl_(new impl(s)) { impl_->defaults(); }

url::url(const std::string &s, bool d) : impl_(new impl(s)) { if (d) impl_->defaults(); }

url::url(const url& u) : impl_(new impl(u)) {}

url::~url() {}

url& url::operator=(const url& u) {
    if (this != &u) {
        impl_.reset(new impl(*u.impl_));
    }
    return *this;
}

url::operator std::string() const { return *impl_; }

std::string url::scheme() const { return str(impl_->scheme); }
std::string url::user() const { return str(impl_->username); }
std::string url::password() const { return str(impl_->password); }
std::string url::host() const { return str(impl_->host); }
std::string url::port() const { return str(impl_->port); }
std::string url::path() const { return str(impl_->path); }

std::string url::host_port() const { return host() + ":" + port(); }

bool url::empty() const { return impl_->str.empty(); }

const std::string url::AMQP("amqp");
const std::string url::AMQPS("amqps");

uint16_t url::port_int() const {
    if (port() == AMQP) return 5672;
    if (port() == AMQPS) return 5671;
    std::istringstream is(port());
    uint16_t result;
    is >> result;
    if (is.fail())
        throw url_error("invalid port '" + port() + "'");
    return result;
}

std::ostream& operator<<(std::ostream& o, const url& u) {
    return o << std::string(u);
}

std::string to_string(const url& u) {
    return u;
}

std::istream& operator>>(std::istream& i, url& u) {
    std::string s;
    i >> s;
    if (!i.fail() && !i.bad()) {
        if (!s.empty()) {
            url::impl* p = new url::impl(s);
            p->defaults();
            u.impl_.reset(p);
        } else {
            i.clear(std::ios::failbit);
        }
    }
    return i;
}

} // namespace proton
