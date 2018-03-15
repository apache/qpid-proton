#ifndef PROTON_DURATION_HPP
#define PROTON_DURATION_HPP

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

#include "./internal/export.hpp"
#include "./internal/comparable.hpp"
#include "./types_fwd.hpp"

#include <proton/type_compat.h>

#include <iosfwd>

/// @file
/// @copybrief proton::duration

namespace proton {

/// A span of time in milliseconds.
class duration : private internal::comparable<duration> {
  public:
    /// A numeric type holding a value in milliseconds.
    typedef int64_t numeric_type;

    /// Construct from a value in milliseconds.
    explicit duration(numeric_type ms = 0) : ms_(ms) {}

    /// Assign a value in milliseconds.
    duration& operator=(numeric_type ms) { ms_ = ms; return *this; }

    /// Get the value in milliseconds.
    numeric_type milliseconds() const { return ms_; }

    PN_CPP_EXTERN static const duration FOREVER;   ///< Wait forever
    PN_CPP_EXTERN static const duration IMMEDIATE; ///< Don't wait at all
    PN_CPP_EXTERN static const duration SECOND;    ///< One second
    PN_CPP_EXTERN static const duration MILLISECOND; ///< One millisecond
    PN_CPP_EXTERN static const duration MINUTE;    ///< One minute

  private:
    numeric_type ms_;
};

/// Print a duration.
PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, duration);

/// @name Comparison and arithmetic operators
/// @{
inline bool operator<(duration x, duration y) { return x.milliseconds() < y.milliseconds(); }
inline bool operator==(duration x, duration y) { return x.milliseconds() == y.milliseconds(); }

inline duration operator+(duration x, duration y) { return duration(x.milliseconds() + y.milliseconds()); }
inline duration operator-(duration x, duration y) { return duration(x.milliseconds() - y.milliseconds()); }
inline duration operator*(duration d, uint64_t n) { return duration(d.milliseconds()*n); }
inline duration operator*(uint64_t n, duration d) { return d * n; }
inline duration operator/(duration d, uint64_t n) { return duration(d.milliseconds() / n); }
/// @}

} // proton

#endif // PROTON_DURATION_HPP
