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

#include <proton/link.h>
#include <proton/handlers.h>
#include <assert.h>

typedef struct {
  int window;
  int drained;
} pni_flowcontroller_t;

pni_flowcontroller_t *pni_flowcontroller(pn_handler_t *handler) {
  return (pni_flowcontroller_t *) pn_handler_mem(handler);
}

static void pni_topup(pn_link_t *link, int window) {
  int delta = window - pn_link_credit(link);
  pn_link_flow(link, delta);
}

static void pn_flowcontroller_dispatch(pn_handler_t *handler, pn_event_t *event, pn_event_type_t type) {
  pni_flowcontroller_t *fc = pni_flowcontroller(handler);
  int window = fc->window;
  pn_link_t *link = pn_event_link(event);

  switch (pn_event_type(event)) {
  case PN_LINK_LOCAL_OPEN:
  case PN_LINK_REMOTE_OPEN:
  case PN_LINK_FLOW:
  case PN_DELIVERY:
    if (pn_link_is_receiver(link)) {
      fc->drained += pn_link_drained(link);
      if (!fc->drained) {
        pni_topup(link, window);
      }
    }
    break;
  default:
    break;
  }
}

pn_flowcontroller_t *pn_flowcontroller(int window) {
  // XXX: a window of 1 doesn't work because we won't necessarily get
  // notified when the one allowed delivery is settled
  assert(window > 1);
  pn_flowcontroller_t *handler = pn_handler_new(pn_flowcontroller_dispatch, sizeof(pni_flowcontroller_t), NULL);
  pni_flowcontroller_t *fc = pni_flowcontroller(handler);
  fc->window = window;
  fc->drained = 0;
  return handler;
}
