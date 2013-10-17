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

#include "messenger.h"

struct pn_subscription_t {
  pn_messenger_t *messenger;
  pn_string_t *scheme;
  void *context;
};

void pn_subscription_initialize(void *obj)
{
  pn_subscription_t *sub = (pn_subscription_t *) obj;
  sub->messenger = NULL;
  sub->scheme = pn_string(NULL);
  sub->context = NULL;
}

void pn_subscription_finalize(void *obj)
{
  pn_subscription_t *sub = (pn_subscription_t *) obj;
  pn_free(sub->scheme);
}

#define pn_subscription_hashcode NULL
#define pn_subscription_compare NULL
#define pn_subscription_inspect NULL

pn_subscription_t *pn_subscription(pn_messenger_t *messenger, const char *scheme)
{
  static pn_class_t clazz = PN_CLASS(pn_subscription);
  pn_subscription_t *sub = (pn_subscription_t *) pn_new(sizeof(pn_subscription_t), &clazz);
  sub->messenger = messenger;
  pn_string_set(sub->scheme, scheme);
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
