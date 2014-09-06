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

#include <proton/messenger.h>
#include <proton/object.h>
#include <assert.h>
#include <string.h>

#include "messenger.h"

struct pn_subscription_t {
  pn_messenger_t *messenger;
  pn_string_t *scheme;
  pn_string_t *host;
  pn_string_t *port;
  pn_string_t *address;
  void *context;
};

void pn_subscription_initialize(void *obj)
{
  pn_subscription_t *sub = (pn_subscription_t *) obj;
  sub->messenger = NULL;
  sub->scheme = pn_string(NULL);
  sub->host = pn_string(NULL);
  sub->port = pn_string(NULL);
  sub->address = pn_string(NULL);
  sub->context = NULL;
}

void pn_subscription_finalize(void *obj)
{
  pn_subscription_t *sub = (pn_subscription_t *) obj;
  pn_free(sub->scheme);
  pn_free(sub->host);
  pn_free(sub->port);
  pn_free(sub->address);
}

#define pn_subscription_hashcode NULL
#define pn_subscription_compare NULL
#define pn_subscription_inspect NULL

pn_subscription_t *pn_subscription(pn_messenger_t *messenger,
                                   const char *scheme,
                                   const char *host,
                                   const char *port)
{
  static const pn_class_t clazz = PN_CLASS(pn_subscription);
  pn_subscription_t *sub = (pn_subscription_t *) pn_new(sizeof(pn_subscription_t), &clazz);
  sub->messenger = messenger;
  pn_string_set(sub->scheme, scheme);
  pn_string_set(sub->host, host);
  pn_string_set(sub->port, port);
  pni_messenger_add_subscription(messenger, sub);
  pn_decref(sub);
  return sub;
}

const char *pn_subscription_scheme(pn_subscription_t *sub)
{
  assert(sub);
  return pn_string_get(sub->scheme);
}

void *pn_subscription_get_context(pn_subscription_t *sub)
{
  assert(sub);
  return sub->context;
}

void pn_subscription_set_context(pn_subscription_t *sub, void *context)
{
  assert(sub);
  sub->context = context;
}

int pni_subscription_set_address(pn_subscription_t *sub, const char *address)
{
  assert(sub);

  if (!address) return 0;

  bool absolute = strncmp(address, "amqp:", 5) == 0;

  if (absolute) {
    return pn_string_set(sub->address, address);
  } else {
    pn_string_set(sub->address, "");
    bool scheme = pn_string_get(sub->scheme);
    if (scheme) {
      int e = pn_string_addf(sub->address, "%s:", pn_string_get(sub->scheme));
      if (e) return e;
    }
    if (pn_string_get(sub->host)) {
      int e = pn_string_addf(sub->address, scheme ? "//%s" : "%s", pn_string_get(sub->host));
      if (e) return e;
    }
    if (pn_string_get(sub->port)) {
      int e = pn_string_addf(sub->address, ":%s", pn_string_get(sub->port));
      if (e) return e;
    }
    return pn_string_addf(sub->address, "/%s", address);
  }
}

const char *pn_subscription_address(pn_subscription_t *sub)
{
  assert(sub);
  while (!pn_string_get(sub->address)) {
    int err = pni_messenger_work(sub->messenger);
    if (err < 0) {
      return NULL;
    }
  }
  return pn_string_get(sub->address);
}
