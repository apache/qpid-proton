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

#include <proton/object.h>
#include <proton/event.h>
#include <stdlib.h>

#define assert(E) ((E) ? 0 : (abort(), 0))

static void test_collector(void) {
  pn_collector_t *collector = pn_collector();
  assert(collector);
  pn_free(collector);
}

#define SETUP_COLLECTOR \
  void *obj = pn_class_new(PN_OBJECT, 0); \
  pn_collector_t *collector = pn_collector(); \
  assert(collector); \
  pn_event_t *event = pn_collector_put(collector, PN_OBJECT, obj, (pn_event_type_t) 0); \
  pn_decref(obj); \

static void test_collector_put(void) {
  SETUP_COLLECTOR;
  assert(event);
  assert(pn_event_context(event) == obj);
  pn_free(collector);
}

static void test_collector_peek(void) {
  SETUP_COLLECTOR;
  pn_event_t *head = pn_collector_peek(collector);
  assert(head == event);
  pn_free(collector);
}

static void test_collector_pop(void) {
  SETUP_COLLECTOR;
  pn_event_t *head = pn_collector_peek(collector);
  assert(head == event);
  pn_collector_pop(collector);
  head = pn_collector_peek(collector);
  assert(!head);
  pn_free(collector);
}

static void test_collector_pool(void) {
  SETUP_COLLECTOR;
  pn_event_t *head = pn_collector_peek(collector);
  assert(head == event);
  pn_collector_pop(collector);
  head = pn_collector_peek(collector);
  assert(!head);
  void *obj2 = pn_class_new(PN_OBJECT, 0);
  pn_event_t *event2 = pn_collector_put(collector, PN_OBJECT, obj2, (pn_event_type_t) 0);
  pn_decref(obj2);
  assert(event == event2);
  pn_free(collector);
}

static void test_event_incref(bool eventfirst) {
  SETUP_COLLECTOR;
  pn_event_t *head = pn_collector_peek(collector);
  assert(head == event);
  pn_incref(head);
  pn_collector_pop(collector);
  assert(!pn_collector_peek(collector));
  void *obj2 = pn_class_new(PN_OBJECT, 0);
  pn_event_t *event2 = pn_collector_put(collector, PN_OBJECT, obj2, (pn_event_type_t) 0);
  pn_decref(obj2);
  assert(head != event2);
  if (eventfirst) {
    pn_decref(head);
    pn_free(collector);
  } else {
    pn_free(collector);
    pn_decref(head);
  }
}

int main(int argc, char **argv)
{
  test_collector();
  test_collector_put();
  test_collector_peek();
  test_collector_pop();
  test_collector_pool();
  test_event_incref(true);
  test_event_incref(false);
  return 0;
}
