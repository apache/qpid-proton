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

struct pn_flowcontroller_t {
  int window;
};

static void pni_topup(pn_link_t *link, int window) {
  int delta = window - pn_link_credit(link);
  pn_link_flow(link, delta);
}

static void pn_flowcontroller_dispatch(pn_handler_t *handler, pn_event_t *event) {
  pn_flowcontroller_t *fc = (pn_flowcontroller_t *) pn_handler_mem(handler);
  int window = fc->window;

  switch (pn_event_type(event)) {
  case PN_LINK_LOCAL_OPEN:
  case PN_LINK_REMOTE_OPEN:
  case PN_LINK_FLOW:
  case PN_DELIVERY:
    {
      pn_link_t *link = pn_event_link(event);
      if (pn_link_is_receiver(link)) {
        pni_topup(link, window);
      }
    }
    break;
  default:
    break;
  }
}

pn_flowcontroller_t *pn_flowcontroller(int window) {
  pn_handler_t *handler = pn_handler_new(pn_flowcontroller_dispatch, sizeof(pn_flowcontroller_t), NULL);
  pn_flowcontroller_t *flowcontroller = (pn_flowcontroller_t *) pn_handler_mem(handler);
  flowcontroller->window = window;
  return flowcontroller;
}

pn_handler_t *pn_flowcontroller_handler(pn_flowcontroller_t *flowcontroller) {
  return pn_handler_cast(flowcontroller);
}
