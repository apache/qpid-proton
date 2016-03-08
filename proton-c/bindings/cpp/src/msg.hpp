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

/** A simple facade for std::ostringstream that allows
 * in place construction of a message and automatic conversion
 * to string.
 * E.g.
 *@code
 * void foo(const std::string&);
 * foo(msg() << "hello " << 32);
 *@endcode
 * Will construct the string "hello 32" and pass it to foo()
 */
struct msg {
    std::ostringstream os;
    msg() {}
    msg(const msg& m) : os(m.str()) {}
    std::string str() const { return os.str(); }
    operator std::string() const { return str(); }
    template <class T> msg& operator<<(const T& t) { os << t; return *this; }
};

inline std::ostream& operator<<(std::ostream& o, const msg& m) { return o << m.str(); }

/** Construct a message using operator << and append (file:line) */
#define QUOTe_(x) #x
#define QUOTE(x) QUOTe_(x)
#define MSG(message) (::proton::msg() << message)

}

#endif  /*!PROTON_MSG_H*/
