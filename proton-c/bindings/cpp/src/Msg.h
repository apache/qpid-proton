#ifndef PROTON_MSG_H
#define PROTON_MSG_H

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

#include <sstream>
#include <iostream>

namespace proton {
namespace reactor {

/** A simple wrapper for std::ostringstream that allows
 * in place construction of a message and automatic conversion
 * to string.
 * E.g.
 *@code
 * void foo(const std::string&);
 * foo(Msg() << "hello " << 32);
 *@endcode
 * Will construct the string "hello 32" and pass it to foo()
 */
struct Msg {
    std::ostringstream os;
    Msg() {}
    Msg(const Msg& m) : os(m.str()) {}
    std::string str() const { return os.str(); }
    operator std::string() const { return str(); }

    Msg& operator<<(long n) { os << n; return *this; }
    Msg& operator<<(unsigned long n) { os << n; return *this; }
    Msg& operator<<(bool n) { os << n; return *this; }
    Msg& operator<<(short n) { os << n; return *this; }
    Msg& operator<<(unsigned short n) { os << n; return *this; }
    Msg& operator<<(int n) { os << n; return *this; }
    Msg& operator<<(unsigned int n) { os << n; return *this; }
#ifdef _GLIBCXX_USE_LONG_LONG
    Msg& operator<<(long long n) { os << n; return *this; }
    Msg& operator<<(unsigned long long n) { os << n; return *this; }
#endif
    Msg& operator<<(double n) { os << n; return *this; }
    Msg& operator<<(float n) { os << n; return *this; }
    Msg& operator<<(long double n) { os << n; return *this; }

    template <class T> Msg& operator<<(const T& t) { os <<t; return *this; }
};



inline std::ostream& operator<<(std::ostream& o, const Msg& m) {
    return o << m.str();
}

/** Construct a message using operator << and append (file:line) */
#define QUOTE_(x) #x
#define QUOTE(x) QUOTE_(x)
#define MSG(message) (::proton::reactor::Msg() << message << " (" __FILE__ ":" QUOTE(__LINE__) ")")

}} // namespace proton::reactor

#endif  /*!PROTON_MSG_H*/
