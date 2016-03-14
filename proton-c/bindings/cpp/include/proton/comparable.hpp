#ifndef COMPARABLE_HPP
#define COMPARABLE_HPP

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

namespace proton {

///@cond INTERNAL

/// Base class for comparable types with operator< and
/// operator==. Provides remaining operators.
template <class T> class comparable {
    friend bool operator>(const T &a, const T &b) { return b < a; }
    friend bool operator<=(const T &a, const T &b) { return !(a > b); }
    friend bool operator>=(const T &a, const T &b) { return !(a < b); }
    friend bool operator!=(const T &a, const T &b) { return !(a == b); }
};

///@endcond

}

#endif // COMPARABLE_HPP
