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
#include <json/writer.h>

#include <cstdlib>
#include <fstream>
#include <sstream>

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

proton::error err(const string& message) {
    return proton::error("connection configuration: " + message);
}

Value validate(ValueType t, const Value& v, const string& name) {
    if (v.type() != t)
        throw err(msg() << " '" << name << "' expected " << t << ", found " << v.type());
    return v;
}

Value get(ValueType t, const Value& obj, const char *key, const Value& dflt=Value()) {
    Value v = obj.get(key, dflt);
    return v.isNull() ? dflt : validate(t, v, key);
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
    Value sasl = get(objectValue, root, "sasl");
    opts.sasl_enabled(get_bool(sasl, "enable", true));
    opts.sasl_allow_insecure_mechs(get_bool(sasl, "allow_insecure", false));
    if (!sasl.isNull()) {
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
                  validate(stringValue, v, "sasl/mechanisms");
                  if (i > 0) s << " ";
                  s << v.asString();
              }
              opts.sasl_allowed_mechs(s.str().c_str());
              break;
          }
          default:
            throw err(msg() << "'mechanisms' expected string or array, found " << mechs.type());
        }
    }
}

void parse_tls(const string& scheme, Value root, connection_options& opts) {
    Value tls = get(objectValue, root, "tls");
    if (scheme == "amqps") { // TLS is enabled
        ssl::verify_mode mode = ssl::VERIFY_PEER_NAME;
        Value verifyValue = get(booleanValue, tls, "verify");
        if (!verifyValue.empty()) {
            mode = verifyValue.asBool() ? ssl::VERIFY_PEER_NAME : ssl::ANONYMOUS_PEER;
        }
        string ca = get_string(tls, "ca", "");
        string cert = get_string(tls, "cert", "");
        string key = get_string(tls, "key", "");
        if (!cert.empty()) {
            ssl_certificate sc = key.empty() ? ssl_certificate(cert) : ssl_certificate(cert, key);
            opts.ssl_client_options(ssl_client_options(sc, ca, mode));
        } else if (!ca.empty()) {
            opts.ssl_client_options(ssl_client_options(ca, mode));
        } else if (!verifyValue.empty()) {
            opts.ssl_client_options(ssl_client_options(mode));
        } else {
            opts.ssl_client_options(ssl_client_options());
        }
    } else if (!tls.empty()) {
        throw err(msg() << "'tls' object not allowed unless scheme is \"amqps\"");
    }
}

} // namespace

std::string parse(std::istream& is, connection_options& opts) {
    try {
        std::ostringstream addr;

        Value root;
        is >> root;
        validate(objectValue, root, "configuration");

        string scheme = get_string(root, "scheme", "amqps");
        if (scheme != "amqp" && scheme != "amqps") {
            throw err(msg() << "'scheme' must be \"amqp\" or \"amqps\"");
        }

        string host = get_string(root, "host", "localhost");
        opts.virtual_host(host.c_str());
        addr << host << ":";

        Value port = root.get("port", scheme);
        switch (port.type()) {
          case stringValue:
            addr << port.asString(); break;
          case intValue:
          case uintValue:
            addr << port.asUInt(); break;
          default:
            throw err(msg() << "'port' expected string or uint, found " << port.type());
        }

        Value user = get(stringValue, root, "user");
        if (!user.isNull()) opts.user(user.asString());
        Value password = get(stringValue, root, "password");
        if (!password.isNull()) opts.password(password.asString());

        parse_sasl(root, opts);
        parse_tls(scheme, root, opts);

        return addr.str();
    } catch (const std::exception& e) {
        throw err(e.what());
    } catch (...) {
        throw err("unknown error");
    }
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
    throw err("no default configuration");
}

string parse_default(connection_options& opts) {
    string name = default_file();
    std::ifstream f;
    try {
        f.exceptions(std::ifstream::badbit|std::ifstream::failbit);
        f.open(name.c_str());
    } catch (const std::exception& e) {
        throw err(msg() << "error opening '" << name << "': " << e.what());
    }
    try {
        return parse(f, opts);
    } catch (const std::ifstream::failure& e) {
        throw err(msg() << "io error parsing '" << name << "': " << e.what());
    } catch (const std::exception& e) {
        throw err(msg() << "error parsing '" << name << "': " << e.what());
    } catch (...) {
        throw err(msg() << "error parsing '" << name << "'");
    }
}

}} // namespace proton::connect_config
