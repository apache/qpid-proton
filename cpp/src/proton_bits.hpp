#ifndef PROTON_BITS_HPP
#define PROTON_BITS_HPP
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
#include <proton/link.h>
#include <proton/session.h>

#include <string>
#include <iosfwd>

#include "contexts.hpp"

/**@file
 *
 * Assorted internal proton utilities.
 */

struct pn_error_t;

struct pn_data_t;
struct pn_transport_t;
struct pn_sasl_t;
struct pn_ssl_t;
struct pn_connection_t;
struct pn_session_t;
struct pn_link_t;
struct pn_delivery_t;
struct pn_condition_t;
struct pn_acceptor_t;
struct pn_terminus_t;
struct pn_reactor_t;
struct pn_record_t;

namespace proton {

namespace internal { class data; }
class transport;
class sasl;
class ssl;
class connection;
class session;
class link;
class sender;
class receiver;
class transfer;
class tracker;
class delivery;
class error_condition;
class acceptor;
class terminus;
class source;
class target;
class reactor;
class messaging_handler;

std::string error_str(long code);

/** Print the error string from pn_error_t, or from code if pn_error_t has no error. */
std::string error_str(pn_error_t*, long code=0);

/** Make a void* inspectable via operator <<. */
struct inspectable { void* value; inspectable(void* o) : value(o) {} };

/** Stream a proton object via pn_inspect. */
std::ostream& operator<<(std::ostream& o, const inspectable& object);

void set_error_condition(const error_condition&, pn_condition_t*);

/// Convert a const char* to std::string, convert NULL to the empty string.
inline std::string str(const char* s) { return s ? s : std::string(); }

namespace internal {

// These traits relate the wrapped and wrapper classes for the templated factories below
template <class T> struct wrapped {};
template <> struct wrapped<internal::data> { typedef pn_data_t type; };
template <> struct wrapped<transport> { typedef pn_transport_t type; };
template <> struct wrapped<connection> { typedef pn_connection_t type; };
template <> struct wrapped<session> { typedef pn_session_t type; };
template <> struct wrapped<link> { typedef pn_link_t type; };
template <> struct wrapped<sender> { typedef pn_link_t type; };
template <> struct wrapped<receiver> { typedef pn_link_t type; };
template <> struct wrapped<transfer> { typedef pn_delivery_t type; };
template <> struct wrapped<tracker> { typedef pn_delivery_t type; };
template <> struct wrapped<delivery> { typedef pn_delivery_t type; };
template <> struct wrapped<error_condition> { typedef pn_condition_t type; };
template <> struct wrapped<terminus> { typedef pn_terminus_t type; };
template <> struct wrapped<source> { typedef pn_terminus_t type; };
template <> struct wrapped<target> { typedef pn_terminus_t type; };

template <class T> struct wrapper {};
template <> struct wrapper<pn_data_t> { typedef internal::data type; };
template <> struct wrapper<pn_transport_t> { typedef transport type; };
template <> struct wrapper<pn_connection_t> { typedef connection type; };
template <> struct wrapper<pn_session_t> { typedef session type; };
template <> struct wrapper<pn_link_t> { typedef link type; };
template <> struct wrapper<pn_delivery_t> { typedef transfer type; };
template <> struct wrapper<pn_condition_t> { typedef error_condition type; };
template <> struct wrapper<pn_terminus_t> { typedef terminus type; };

// Factory for wrapper types
template <class T>
class factory {
public:
    static T wrap(typename wrapped<T>::type* t) { return t; }
    static typename wrapped<T>::type* unwrap(const T& t) { return t.pn_object(); }
};

template <class T> struct context {};
template <> struct context<link> {typedef link_context type; };
template <> struct context<receiver> {typedef link_context type; };
template <> struct context<sender> {typedef link_context type; };
template <> struct context<session> {typedef session_context type; };
template <> struct context<connection> {typedef connection_context type; };

template <class T>
inline void set_messaging_handler(T t, messaging_handler* mh) { context<T>::type::get(factory<T>::unwrap(t)).handler = mh; }

template <class T>
inline messaging_handler* get_messaging_handler(T* t) { return context<typename internal::wrapper<T>::type>::type::get(t).handler; }

class returned_factory {
  public:
    template <class T> static returned<T> make(typename internal::wrapped<T>::type* pn) {
        return returned<T>(pn);
    }
};

} // namespace internal

template <class T>
typename internal::wrapper<T>::type make_wrapper(T* t) { return internal::factory<typename internal::wrapper<T>::type>::wrap(t); }

template <class U>
U make_wrapper(typename internal::wrapped<U>::type* t) { return internal::factory<U>::wrap(t); }

template <class T>
typename internal::wrapped<T>::type* unwrap(const T& t) { return internal::factory<T>::unwrap(t); }

template <class T> returned<T> make_returned(typename internal::wrapped<T>::type* pn) {
    return internal::returned_factory::make<T>(pn);
}

// Get an AMQP "multiple" field from a value. A "multiple" field can be encoded as a single
// value or as an array. This function always extracts it as a sequence, a sequence of one
// if it is encoded as a single value.
//
// T should be a valid sequence type for proton::get() with a T::value_type typedef.
//
template<class T>
void get_multiple(const value& v, T& x) {
    if (v.empty()) {
        x.clear();
    } else if (v.type() == ARRAY) {
        proton::get(v,x);
    } else {
        x.resize(1);
        proton::get(v,x[0]);
    }
}

// Same as previous but returns the value.
template<class T>
T get_multiple(const value& v) {
    T x;
    get_multiple(v, x);
    return x;
}

} // namespace proton

#endif // PROTON_BITS_HPP
