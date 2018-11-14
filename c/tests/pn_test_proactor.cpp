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

#include "./pn_test_proactor.hpp"

#include <proton/condition.h>
#include <proton/connection.h>
#include <proton/delivery.h>
#include <proton/event.h>
#include <proton/link.h>
#include <proton/listener.h>
#include <proton/message.h>
#include <proton/netaddr.h>
#include <proton/object.h>
#include <proton/proactor.h>
#include <proton/transport.h>

namespace pn_test {

std::string listening_port(pn_listener_t *l) {
  const pn_netaddr_t *na = pn_listener_addr(l);
  char port[PN_MAX_ADDR];
  pn_netaddr_host_port(na, NULL, 0, port, sizeof(port));
  return port;
}

proactor::proactor(struct handler *h)
    : auto_free<pn_proactor_t, pn_proactor_free>(pn_proactor()), handler(h) {}

bool proactor::dispatch(pn_event_t *e) {
  void *ctx = NULL;
  if (pn_event_listener(e))
    ctx = pn_listener_get_context(pn_event_listener(e));
  else if (pn_event_connection(e))
    ctx = pn_connection_get_context(pn_event_connection(e));
  struct handler *h = ctx ? reinterpret_cast<struct handler *>(ctx) : handler;
  bool ret = h ? h->dispatch(e) : false;
  //  pn_test::handler doesn't know about listeners so save listener condition
  //  here.
  if (pn_event_listener(e)) {
    pn_condition_copy(h->last_condition,
                      pn_listener_condition(pn_event_listener(e)));
  }
  return ret;
}

// RAII event batch
struct auto_batch {
  pn_proactor_t *p_;
  pn_event_batch_t *eb_;

  auto_batch(pn_proactor_t *p, pn_event_batch_t *eb) : p_(p), eb_(eb) {}
  ~auto_batch() {
    if (eb_) pn_proactor_done(p_, eb_);
  }
  pn_event_t *next() { return eb_ ? pn_event_batch_next(eb_) : NULL; }
  bool null() { return !eb_; }
};

std::pair<int, pn_event_type_t> proactor::dispatch(pn_event_batch_t *eb,
                                                   pn_event_type_t stop) {
  auto_batch b(*this, eb);
  pn_event_t *e;
  int n = 0;
  while ((e = b.next())) {
    ++n;
    pn_event_type_t et = pn_event_type(e);
    if (dispatch(e) || et == stop) return std::make_pair(n, et);
  }
  return std::make_pair(n, PN_EVENT_NONE);
}

pn_event_type_t proactor::run(pn_event_type_t stop, bool wait) {
  std::pair<int, pn_event_type_t> result;
  if (wait) {
    result = dispatch(pn_proactor_wait(*this), stop);
  }
  // If we did something but we still don't have an event, try again.
  while (result.first && !result.second) {
    result = dispatch(pn_proactor_get(*this), stop);
  }
  return result.second;
}

pn_event_type_t proactor::corun(proactor &other, pn_event_type_t stop,
                                bool wait) {
  pn_event_type_t et = run(stop, wait);
  int try_again = 100;
  while (!et && try_again) {
    pn_event_type_t ot = other.run(PN_EVENT_NONE, wait);
    et = run(stop, wait);
  }
  return et;
}

pn_event_type_t proactor::wait_next() {
  auto_batch b(*this, pn_proactor_wait(*this));
  pn_event_t *e = b.next();
  if (e) {
    dispatch(e);
    return pn_event_type(e);
  } else {
    return PN_EVENT_NONE;
  }
}

pn_listener_t *proactor::listen(const std::string &addr,
                                struct handler *handler) {
  pn_listener_t *l = pn_listener();
  pn_listener_set_context(l, handler);
  pn_proactor_listen(*this, l, addr.c_str(), 4);
  return l;
}

pn_connection_t *proactor::connect(const std::string &addr, struct handler *h,
                                   pn_connection_t *c) {
  if (!c) c = pn_connection();
  if (h) pn_connection_set_context(c, h);
  pn_proactor_connect2(*this, c, NULL, addr.c_str());
  return c;
}

} // namespace pn_test
