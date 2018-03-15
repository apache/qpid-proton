#ifndef PROTON_TIMESTAMP_HPP
#define PROTON_TIMESTAMP_HPP

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

#include "./duration.hpp"

#include <proton/type_compat.h>

/// @file
/// @copybrief proton::timestamp

namespace proton {

/// A 64-bit timestamp in milliseconds since the Unix epoch.
///    
/// The dawn of the Unix epoch was 00:00:00 (UTC), 1 January 1970.
class timestamp : private internal::comparable<timestamp> {
  public:
    /// A numeric type holding a value in milliseconds.
    typedef int64_t numeric_type;

    /// The current wall-clock time.
    PN_CPP_EXTERN static timestamp now();

    /// Construct from a value in milliseconds.
    explicit timestamp(numeric_type ms = 0) : ms_(ms) {}

    /// Assign a value in milliseconds.
    timestamp& operator=(numeric_type ms) { ms_ = ms; return *this; }

    /// Get the value in milliseconds.
    numeric_type milliseconds() const { return ms_; }

  private:
    numeric_type ms_;
};

/// @name Comparison and arithmetic operators
/// @{
inline bool operator==(timestamp x, timestamp y) { return x.milliseconds() == y.milliseconds(); }
inline bool operator<(timestamp x, timestamp y) { return x.milliseconds() < y.milliseconds(); }

inline timestamp operator+(timestamp ts, duration d) { return timestamp(ts.milliseconds() + d.milliseconds()); }
inline timestamp operator-(timestamp ts, duration d) { return timestamp(ts.milliseconds() - d.milliseconds()); }
inline duration operator-(timestamp t0, timestamp t1) { return duration(t0.milliseconds() - t1.milliseconds()); }
inline timestamp operator+(duration d, timestamp ts) { return ts + d; }
/// @}

/// Print a timestamp.
PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, timestamp);

} // proton

#endif // PROTON_TIMESTAMP_HPP
