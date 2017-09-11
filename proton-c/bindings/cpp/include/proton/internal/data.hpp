#ifndef PROTON_INTERNAL_DATA_HPP
#define PROTON_INTERNAL_DATA_HPP

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

#include "../internal/object.hpp"
#include "../types_fwd.hpp"

struct pn_data_t;

namespace proton {

class value;

namespace internal {

/// @cond INTERNAL
/// A wrapper for a proton data object.
class data : public object<pn_data_t> {
    /// Wrap an existing Proton-C data object.
    data(pn_data_t* d) : internal::object<pn_data_t>(d) {}

  public:
    /// Create an empty data object.
    data() : internal::object<pn_data_t>(0) {}

    /// Create a new data object.
    PN_CPP_EXTERN static data create();

    /// Copy the contents of another data object.
    PN_CPP_EXTERN void copy(const data&);

    /// Clear the data.
    PN_CPP_EXTERN void clear();

    /// Rewind current position to the start.
    PN_CPP_EXTERN void rewind();

    /// True if there are no values.
    PN_CPP_EXTERN bool empty() const;

    /// Append the contents of another data object.
    PN_CPP_EXTERN int append(data src);

    /// Append up to limit items from data object.
    PN_CPP_EXTERN int appendn(data src, int limit);

    PN_CPP_EXTERN bool next();
    PN_CPP_EXTERN const void* point() const;
    PN_CPP_EXTERN void restore(const void* h);

  protected:
    void narrow();
    void widen();

  friend class internal::factory<data>;
  friend struct state_guard;
  friend PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const data&);
};
/// @endcond

/// **Unsettled API** - Save and restore codec state.
///
/// A state guard saves the state and restores it in the destructor
/// unless `cancel()` is called.
struct state_guard {
    /// @cond INTERNAL
    data& data_;
    const void* point_;
    bool cancel_;
    /// @endcond

    /// @cond INTERNAL
    state_guard(data& d) : data_(d), point_(data_.point()), cancel_(false) {}
    /// @endcond

    ~state_guard() { if (!cancel_) data_.restore(point_); }

    /// Discard the saved state.
    void cancel() { cancel_ = true; }
};

} // internal
} // proton

#endif // PROTON_INTERNAL_DATA_HPP
