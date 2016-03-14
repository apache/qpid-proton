#ifndef PROTON_CPP_DURATION_H
#define PROTON_CPP_DURATION_H

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

#include <proton/export.hpp>
#include <proton/comparable.hpp>
#include <proton/types_fwd.hpp>

#include <iosfwd>

namespace proton {

/// A span of time in milliseconds.
class duration : private comparable<duration> {
  public:
    typedef uint64_t numeric_type; ///< Numeric type used to store milliseconds

    explicit duration(numeric_type ms = 0) : ms_(ms) {} ///< Construct from milliseconds
    duration& operator=(numeric_type ms) { ms_ = ms; return *this; } ///< Assign

    numeric_type milliseconds() const { return ms_; } ///< Return milliseconds
    numeric_type ms() const { return ms_; }           ///< Return milliseconds

    PN_CPP_EXTERN static const duration FOREVER;   ///< Wait for ever
    PN_CPP_EXTERN static const duration IMMEDIATE; ///< Don't wait at all
    PN_CPP_EXTERN static const duration SECOND;    ///< One second
    PN_CPP_EXTERN static const duration MINUTE;    ///< One minute

  private:
    numeric_type ms_;
};

/// Print duration
PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, duration);

///@name Comparison and arithmetic operators
///@{
inline bool operator<(duration x, duration y) { return x.ms() < y.ms(); }
inline bool operator==(duration x, duration y) { return x.ms() == y.ms(); }

inline duration operator+(duration x, duration y) { return duration(x.ms() + y.ms()); }
inline duration operator-(duration x, duration y) { return duration(x.ms() - y.ms()); }
inline duration operator*(duration d, uint64_t n) { return duration(d.ms()*n); }
inline duration operator*(uint64_t n, duration d) { return d * n; }
///@}
}

#endif // PROTON_CPP_DURATION_H
