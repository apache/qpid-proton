#ifndef TESTS_PN_TEST_HPP
#define TESTS_PN_TEST_HPP
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

/// @file
///
/// Wrappers and Catch2 extensions to simplify testing the proton-C library.

#include <catch_extra.hpp>

#include <proton/condition.h>
#include <proton/connection_driver.h>
#include <proton/event.h>
#include <proton/message.h>

#include <iosfwd>
#include <string>
#include <vector>

// String form of C pn_ types used in tests, must be in global C namespace
// Note objects are passed by reference, not pointer.
std::ostream &operator<<(std::ostream &, pn_event_type_t);
std::ostream &operator<<(std::ostream &, const pn_condition_t &);
std::ostream &operator<<(std::ostream &, const pn_error_t &);

namespace pn_test {

// Holder for T*, calls function Free() in dtor. Not copyable.
template <class T, void (*Free)(T *)> class auto_free {
  T *ptr_;
  auto_free &operator=(auto_free &x);
  auto_free(auto_free &x);

public:
  auto_free(T *p = 0) : ptr_(p) {}
  ~auto_free() { Free(ptr_); }
  T *get() const { return ptr_; }
  operator T *() const { return ptr_; }
};

// pn_free() works for some, but not all pn_xxx_t* types.
// Add typed pn_string_free() so we can be consistent and safe.
inline void pn_string_free(pn_string_t *s) { pn_free(s); }

// Call pn_inspect(), return std::string
std::string inspect(void *);

// List of pn_event_type_t
typedef std::vector<pn_event_type_t> etypes;
std::ostream &operator<<(std::ostream &o, const etypes &et);

// Workaround for lack of list initializers in C++03.
// Use ETYPES macro, don't call make_etypes_ directly
etypes make_etypes_(int first, ...);
#define ETYPES(...) (make_etypes_(__VA_ARGS__, -1))

/// Make a pn_bytes_t from a std::string
pn_bytes_t pn_bytes(const std::string &s);

/// Ensure buf has at least size bytes, use realloc if need be
void rwbytes_ensure(pn_rwbytes_t *buf, size_t size);

/// Decode message from delivery into buf, expand buf as needed.
void message_decode(pn_message_t *m, pn_delivery_t *d, pn_rwbytes_t *buf);

// A test handler that logs the type of each event handled, and has
// slots to store all of the basic proton types for ad-hoc use in
// tests. Subclass and override the handle() method.
struct handler {
  etypes log; // Log of events
  auto_free<pn_condition_t, pn_condition_free>
      last_condition; // Condition of last event

  // Slots to save proton objects for use outside the handler.
  pn_listener_t *listener;
  pn_connection_t *connection;
  pn_session_t *session;
  pn_link_t *link;
  pn_link_t *sender;
  pn_link_t *receiver;
  pn_delivery_t *delivery;
  pn_message_t *message;

  handler();

  /// dispatch an event: log its type then call handle()
  /// Returns the value of handle()
  bool dispatch(pn_event_t *e);

  // Return the current log contents, clear the log.
  etypes log_clear();

  // Return the last event in the log, clear the log.
  pn_event_type_t log_last();

protected:
  // Override this function to handle events.
  //
  // Return true to stop dispatching and return control to the test function,
  // false to continue processing.
  virtual bool handle(pn_event_t *e) { return false; }
};

// A pn_connection_driver_t that dispatches to a pn_test::handler
//
// driver::run() dispatches events to the handler, but returns if the handler
// returns true, or if a specific event type is handled. Test functions can
// alternate between letting the handler run and checking state or calling
// functions on proton objects in the test function directly. Handlers can
// automate uninteresting work, the test function can make checks that are
// clearly located in the flow of the test logic.
struct driver : public ::pn_connection_driver_t {
  struct handler &handler;

  driver(struct handler &h);
  ~driver();

  // Dispatch events till a handler returns true, the `stop` event is handled,
  // or there are no more events
  // Returns the last event handled or PN_EVENT_NONE if none were.
  pn_event_type_t run(pn_event_type_t stop = PN_EVENT_NONE);

  // Transfer available data from src write buffer to this read-buffer and
  // update both drivers. Return size of data transferred.
  size_t read(pn_connection_driver_t &src);
};

// A client/server pair drivers. run() simulates a connection in memory.
struct driver_pair {
  driver client, server;

  // Associate handlers with drivers. Sets server.transport to server mode
  driver_pair(handler &ch, handler &sh);

  // Run the drivers until a handle returns true or there is nothing left to
  // do. Opens the client.connection() if not already open.
  // Return the last event handled or PN_EVENT_NONE
  pn_event_type_t run();
};

// Matches for use with Catch macros CHECK_THAT and REQUIRE_THAT.
// Check pn_condition_t and pn_error_t, failed checks report code, name,
// description etc.

struct cond_empty : public Catch::MatcherBase<pn_condition_t> {
  std::string describe() const CATCH_OVERRIDE;
  bool match(const pn_condition_t &cond) const CATCH_OVERRIDE;
};

class cond_matches : public Catch::MatcherBase<pn_condition_t> {
  std::string name_, desc_;

public:
  cond_matches(const std::string &name, const std::string &desc_contains = "");
  std::string describe() const CATCH_OVERRIDE;
  bool match(const pn_condition_t &cond) const CATCH_OVERRIDE;
};

struct error_empty : public Catch::MatcherBase<pn_error_t> {
  std::string describe() const CATCH_OVERRIDE;
  bool match(const pn_error_t &) const CATCH_OVERRIDE;
};

class error_matches : public Catch::MatcherBase<pn_error_t> {
  int code_;
  std::string desc_;

public:
  error_matches(int code, const std::string &desc_contains = "");
  std::string describe() const CATCH_OVERRIDE;
  bool match(const pn_error_t &) const CATCH_OVERRIDE;
};

} // namespace pn_test

#endif // TESTS_PN_TEST_HPP
