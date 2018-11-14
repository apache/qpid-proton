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

#include "./pn_test.hpp"

#include <proton/event.h>
#include <proton/object.h>

TEST_CASE("event_collector") {
  pn_collector_t *collector = pn_collector();
  REQUIRE(collector);
  pn_free(collector);
}

#define SETUP_COLLECTOR                                                        \
  void *obj = pn_class_new(PN_OBJECT, 0);                                      \
  pn_collector_t *collector = pn_collector();                                  \
  REQUIRE(collector);                                                          \
  pn_event_t *event =                                                          \
      pn_collector_put(collector, PN_OBJECT, obj, (pn_event_type_t)0);         \
  pn_decref(obj);

TEST_CASE("event_collector_put") {
  SETUP_COLLECTOR;
  REQUIRE(event);
  REQUIRE(pn_event_context(event) == obj);
  pn_free(collector);
}

TEST_CASE("event_collector_peek") {
  SETUP_COLLECTOR;
  pn_event_t *head = pn_collector_peek(collector);
  REQUIRE(head == event);
  pn_free(collector);
}

TEST_CASE("event_collector_pop") {
  SETUP_COLLECTOR;
  pn_event_t *head = pn_collector_peek(collector);
  REQUIRE(head == event);
  pn_collector_pop(collector);
  head = pn_collector_peek(collector);
  REQUIRE(!head);
  pn_free(collector);
}

TEST_CASE("event_collector_pool") {
  SETUP_COLLECTOR;
  pn_event_t *head = pn_collector_peek(collector);
  REQUIRE(head == event);
  pn_collector_pop(collector);
  head = pn_collector_peek(collector);
  REQUIRE(!head);
  void *obj2 = pn_class_new(PN_OBJECT, 0);
  pn_event_t *event2 =
      pn_collector_put(collector, PN_OBJECT, obj2, (pn_event_type_t)0);
  pn_decref(obj2);
  REQUIRE(event == event2);
  pn_free(collector);
}

void test_event_incref(bool eventfirst) {
  INFO("eventfirst = " << eventfirst);
  SETUP_COLLECTOR;
  pn_event_t *head = pn_collector_peek(collector);
  REQUIRE(head == event);
  pn_incref(head);
  pn_collector_pop(collector);
  REQUIRE(!pn_collector_peek(collector));
  void *obj2 = pn_class_new(PN_OBJECT, 0);
  pn_event_t *event2 =
      pn_collector_put(collector, PN_OBJECT, obj2, (pn_event_type_t)0);
  pn_decref(obj2);
  REQUIRE(head != event2);
  if (eventfirst) {
    pn_decref(head);
    pn_free(collector);
  } else {
    pn_free(collector);
    pn_decref(head);
  }
}

TEST_CASE("event_incref") {
  test_event_incref(true);
  test_event_incref(false);
}
