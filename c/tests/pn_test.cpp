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

#include "./pn_test.hpp"

#include <proton/condition.h>
#include <proton/connection.h>
#include <proton/delivery.h>
#include <proton/event.h>
#include <proton/link.h>
#include <proton/message.h>
#include <proton/netaddr.h>
#include <proton/object.h>
#include <proton/transport.h>

std::ostream &operator<<(std::ostream &o, pn_event_type_t et) {
  return o << pn_event_type_name(et);
}

inline std::ostream &quote(std::ostream &o, const char *s) {
  return s ? (o << '"' << s << '"') : (o << "null");
}

std::ostream &operator<<(std::ostream &o, const pn_condition_t &const_cond) {
  pn_condition_t *cond = const_cast<pn_condition_t *>(&const_cond);
  o << "pn_condition{";
  if (pn_condition_is_set(cond)) {
    quote(o, pn_condition_get_name(cond)) << ", ";
    quote(o, pn_condition_get_description(cond));
  }
  o << "}";
  return o;
}

std::ostream &operator<<(std::ostream &o, const pn_error_t &const_err) {
  pn_error_t *err = const_cast<pn_error_t *>(&const_err);
  o << "pn_error{" << pn_code(pn_error_code(err)) << ", ";
  return quote(o, pn_error_text(err)) << "}";
}

namespace pn_test {

std::string inspect(void *obj) {
  auto_free<pn_string_t, pn_string_free> s(pn_string(NULL));
  pn_inspect(obj, s);
  return pn_string_get(s);
}

etypes make_etypes_(int first, ...) {
  etypes v;
  va_list ap;
  va_start(ap, first);
  for (int i = first; i >= 0; i = va_arg(ap, int)) {
    v.push_back(static_cast<pn_event_type_t>(i));
  }
  va_end(ap);
  return v;
}

std::ostream &operator<<(std::ostream &o, const etypes &et) {
  return o << Catch::toString(static_cast<std::vector<pn_event_type_t> >(et));
}

pn_bytes_t pn_bytes(const std::string &s) {
  return ::pn_bytes(s.size(), s.data());
}

void rwbytes_ensure(pn_rwbytes_t *buf, size_t size) {
  if (buf->start == NULL || buf->size < size) {
    buf->start = (char *)realloc(buf->start, size);
    buf->size = size;
  }
}

void message_decode(pn_message_t *m, pn_delivery_t *d, pn_rwbytes_t *buf) {
  ssize_t size = pn_delivery_pending(d);
  rwbytes_ensure(buf, size);
  ssize_t result = pn_link_recv(pn_delivery_link(d), buf->start, size);
  REQUIRE(size == result);
  pn_message_clear(m);
  if (pn_message_decode(m, buf->start, size)) FAIL(pn_message_error(m));
}

handler::handler()
    : last_condition(pn_condition()), connection(), session(), link(), sender(),
      receiver(), delivery(), message() {}

bool handler::dispatch(pn_event_t *e) {
  log.push_back(pn_event_type(e));
  if (pn_event_condition(e)) {
    pn_condition_copy(last_condition, pn_event_condition(e));
  } else {
    pn_condition_clear(last_condition);
  }
  return handle(e);
}

etypes handler::log_clear() {
  etypes ret;
  std::swap(ret, log);
  return ret;
}

pn_event_type_t handler::log_last() {
  pn_event_type_t et = log.empty() ? PN_EVENT_NONE : log.back();
  log.clear();
  return et;
}

driver::driver(struct handler &h) : handler(h) {
  pn_connection_driver_init(this, NULL, NULL);
}
driver::~driver() { pn_connection_driver_destroy(this); }

pn_event_type_t driver::run(pn_event_type_t stop) {
  pn_event_t *e = NULL;
  while ((e = pn_connection_driver_next_event(this))) {
    pn_event_type_t et = pn_event_type(e);
    if (handler.dispatch(e) || et == stop) return et;
  }
  return PN_EVENT_NONE;
}

driver_pair::driver_pair(handler &ch, handler &sh) : client(ch), server(sh) {
  pn_transport_set_server(server.transport);
}

size_t driver::read(pn_connection_driver_t &src) {
  pn_bytes_t wb = pn_connection_driver_write_buffer(&src);
  pn_rwbytes_t rb = pn_connection_driver_read_buffer(this);
  size_t size = rb.size < wb.size ? rb.size : wb.size;
  if (size) {
    std::copy(wb.start, wb.start + size, rb.start);
    pn_connection_driver_write_done(&src, size);
    pn_connection_driver_read_done(this, size);
  }
  return size;
}

pn_event_type_t driver_pair::run() {
  pn_connection_open(client.connection); // Make sure it is open
  size_t n = 0;
  do {
    pn_event_type_t et = PN_EVENT_NONE;
    if ((et = client.run())) return et;
    if ((et = server.run())) return et;
    n = client.read(server) + server.read(client);
  } while (n || pn_connection_driver_has_event(&client) ||
           pn_connection_driver_has_event(&server));
  return PN_EVENT_NONE;
}

std::string cond_empty::describe() const { return "is empty"; }

bool cond_empty::match(const pn_condition_t &cond) const {
  return !pn_condition_is_set(const_cast<pn_condition_t *>(&cond));
}

cond_matches::cond_matches(const std::string &name, const std::string &desc)
    : name_(name), desc_(desc) {}

std::string cond_matches::describe() const {
  std::ostringstream o;
  o << "matches " << Catch::toString(name_);
  if (!desc_.empty()) o << ", " + Catch::toString(desc_);
  return o.str();
}

bool cond_matches::match(const pn_condition_t &const_cond) const {
  pn_condition_t *cond = const_cast<pn_condition_t *>(&const_cond);
  const char *name = pn_condition_get_name(cond);
  const char *desc = pn_condition_get_description(cond);
  return pn_condition_is_set(cond) && name && name_ == name &&
         (desc_.empty() || (desc && Catch::contains(desc, desc_)));
}

std::string error_empty::describe() const { return "is empty"; }

bool error_empty::match(const pn_error_t &err) const {
  return !pn_error_code(const_cast<pn_error_t *>(&err));
}

error_matches::error_matches(int code, const std::string &desc)
    : code_(code), desc_(desc) {}

std::string error_matches::describe() const {
  std::ostringstream o;
  o << "matches " << pn_code(code_);
  if (!desc_.empty()) o << ", " + Catch::toString(desc_);
  return o.str();
}

bool error_matches::match(const pn_error_t &const_err) const {
  pn_error_t *err = const_cast<pn_error_t *>(&const_err);
  int code = pn_error_code(err);
  const char *desc = pn_error_text(err);
  return code_ == code &&
         (desc_.empty() || (desc && Catch::contains(desc, desc_)));
}

} // namespace pn_test
