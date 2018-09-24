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

#include "msg.hpp"

#include <proton/connect_config.hpp>
#include <proton/error.hpp>
#include <proton/ssl.hpp>

#include <proton/version.h>

#include <json/value.h>
#include <json/reader.h>

#include <cstdlib>
#include <fstream>

using namespace Json;
using std::string;

namespace {
const char *type_name(ValueType t) {
    switch (t) {
      case nullValue: return "null";
      case intValue: return "int";
      case uintValue: return "uint";
      case realValue: return "real";
      case stringValue: return "string";
      case booleanValue: return "boolean";
      case arrayValue: return "array";
      case objectValue: return "object";
      default: return "unknown";
    }
}
} // namespace

namespace std {
ostream& operator<<(ostream& o, ValueType t) { return o << type_name(t); }
}

namespace proton {
namespace connect_config {

namespace {

void raise(const string& message) {
    throw proton::error("connection configuration: " + message);
}

Value validate(ValueType t, const Value& v, const string& name) {
    if (v.type() != t)
        raise(msg() << " '" << name << "' expected " << t << ", found " << v.type());
    return v;
}

Value get(ValueType t, const Value& obj, const char *key, const Value& dflt=Value()) {
    Value v = (obj.type() != nullValue) ? obj.get(key, dflt) : dflt;
    return validate(t, v, key);
}

bool get_bool(const Value& obj, const char *key, bool dflt) {
    return get(booleanValue, obj, key, dflt).asBool();
}

string get_string(const Value& obj, const char *key, const string& dflt) {
    return get(stringValue, obj, key, dflt).asString();
}

static const string HOME("HOME");
static const string ENV_VAR("MESSAGING_CONNECT_FILE");
static const string FILE_NAME("connect.json");
static const string HOME_FILE_NAME("/.config/messaging/" + FILE_NAME);
static const string ETC_FILE_NAME("/etc/messaging/" + FILE_NAME);

bool exists(const string& name) { return std::ifstream(name.c_str()).good(); }

void parse_sasl(Value root, connection_options& opts) {
    Value sasl = root.get("sasl", Value());
    opts.sasl_enabled(get_bool(sasl, "enable", true));
    if (sasl.type() != nullValue) {
        validate(objectValue, sasl, "sasl");
        opts.sasl_allow_insecure_mechs(get_bool(sasl, "allow_insecure", false));
        Value mechs = sasl.get("mechanisms", Value());
        switch (mechs.type()) {
          case nullValue:
            break;
          case stringValue:
            opts.sasl_allowed_mechs(mechs.asString());
            break;
          case arrayValue: {
              std::ostringstream s;
              for (ArrayIndex i= 0; i < mechs.size(); ++i) {
                  Value v = mechs.get(i, Value());
                  if (v.type() != stringValue) {
                      raise(msg() << "'sasl/mechanisms' expect string elements, found " << v.type());
                  }
                  if (i > 0) s << " ";
                  s << v.asString();
              }
              opts.sasl_allowed_mechs(s.str().c_str());
              break;
          }
          default:
            raise(msg() << "'mechanisms' expected string or array, found " << mechs.type());
        }
    }
}

void parse_tls(const string& scheme, Value root, connection_options& opts) {
    Value tls = root.get("tls", Value());
    if (tls.type() != nullValue) {
        validate(objectValue, tls, "tls");
        if (scheme != "amqps") {
            raise(msg() << "'tls' object is not allowed unless scheme is \"amqps\"");
        }
        string ca = get_string(tls, "ca", "");
        bool verify = get_bool(tls, "verify", true);
        Value cert = get(stringValue, tls, "cert");
        ssl::verify_mode mode = verify ? ssl::VERIFY_PEER_NAME : ssl::ANONYMOUS_PEER;
        if (cert.type() != nullValue) {
            Value key = get(stringValue, tls, "key");
            ssl_certificate cert2 = (key.type() != nullValue) ?
                ssl_certificate(cert.asString(), key.asString()) :
                ssl_certificate(cert.asString());
            opts.ssl_client_options(ssl_client_options(cert2, ca, mode));
        } else {
            ssl_client_options(ssl_client_options(ca, mode));
        }
    }
}

} // namespace

std::string parse(std::istream& is, connection_options& opts) {
    Value root;
    is >> root;

    string scheme = get_string(root, "scheme", "amqps");
    if (scheme != "amqp" && scheme != "amqps") {
        raise(msg() << "'scheme' must be \"amqp\" or \"amqps\"");
    }

    string host = get_string(root, "host", "");
    opts.virtual_host(host.c_str());

    Value port = root.get("port", scheme);
    if (!port.isIntegral() && !port.isString()) {
        raise(msg() << "'port' expected string or integer, found " << port.type());
    }

    Value user = root.get("user", Value());
    if (user.type() != nullValue) opts.user(validate(stringValue, user, "user").asString());
    Value password = root.get("password", Value());
    if (password.type() != nullValue) opts.password(validate(stringValue, password, "password").asString());

    parse_sasl(root, opts);
    parse_tls(scheme, root, opts);
    return host + ":" + port.asString();
}

string default_file() {
    /* Use environment variable if set */
    const char *env_path = getenv(ENV_VAR.c_str());
    if (env_path) return env_path;
    /* current directory */
    if (exists(FILE_NAME)) return FILE_NAME;
    /* $HOME/.config/messaging/FILE_NAME */
    const char *home = getenv(HOME.c_str());
    if (home) {
        string path = home + HOME_FILE_NAME;
        if (exists(path)) return path;
    }
    /* INSTALL_PREFIX/etc/messaging/FILE_NAME */
    if (PN_INSTALL_PREFIX && *PN_INSTALL_PREFIX) {
        string path = PN_INSTALL_PREFIX + ETC_FILE_NAME;
        if (exists(path)) return path;
    }
    /* /etc/messaging/FILE_NAME */
    if (exists(ETC_FILE_NAME)) return ETC_FILE_NAME;
    raise("no default configuration");
    return "";                  // Never get here, keep compiler happy
}

string parse_default(connection_options& opts) {
    string name = default_file();
    std::ifstream f;
    try {
        f.exceptions(~std::ifstream::goodbit);
        f.open(name.c_str());
    } catch (const std::exception& e) {
        raise(msg() << "error opening '" << name << "': " << e.what());
    }
    try {
        return parse(f, opts);
    } catch (const std::exception& e) {
        raise(msg() << "error parsing '" << name << "': " << e.what());
    } catch (...) {
        raise(msg() << "error parsing '" << name);
    }
    return "";                  // Never get here, keep compiler happy
}

}} // namespace proton::connect_config
