#ifndef PROTON_DATA_HPP
#define PROTON_DATA_HPP

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

#include <proton/object.hpp>
#include <proton/types_fwd.hpp>
#include <proton/type_id.hpp>

///@file

struct pn_data_t;

namespace proton {

class value;
///@defgroup codec Internal details of AMQP encoding.
///
/// You can use these classes on an experimental basis to create your own AMQP
/// encodings for C++ types, but they may change in the future. For examples of use
/// see the built-in encodings, for example in proton/vector.hpp or proton/map.hpp

/// @ingroup codec
namespace codec {

/// Wrapper for a proton data object.
class data : public internal::object<pn_data_t> {
  public:
    /// Wrap an existing proton-C data object.
    data(pn_data_t* d=0) : internal::object<pn_data_t>(d) {}

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

  protected:
    PN_CPP_EXTERN void* point() const;
    PN_CPP_EXTERN void restore(void* h);
    PN_CPP_EXTERN void narrow();
    PN_CPP_EXTERN void widen();
    PN_CPP_EXTERN bool next();
    PN_CPP_EXTERN bool prev();

  friend struct state_guard;
  friend PN_CPP_EXTERN std::ostream& operator<<(std::ostream&, const data&);
};

// state_guard saves the state and restores it in dtor unless cancel() is called.
struct state_guard {
    data& data_;
    void* point_;
    bool cancel_;

    state_guard(data& d) : data_(d), point_(data_.point()), cancel_(false) {}
    ~state_guard() { if (!cancel_) data_.restore(point_); }
    void cancel() { cancel_ = true; }
};

// Start encoding a complex type.
struct start {
    start(type_id type_=NULL_TYPE, type_id element_=NULL_TYPE,
          bool described_=false, size_t size_=0) :
        type(type_), element(element_), is_described(described_), size(size_) {}

    type_id type;            ///< The container type: ARRAY, LIST, MAP or DESCRIBED.
    type_id element;         ///< the element type for array only.
    bool is_described;       ///< true if first value is a descriptor.
    size_t size;             ///< the element count excluding the descriptor (if any)

    PN_CPP_EXTERN static start array(type_id element, bool described=false) { return start(ARRAY, element, described); }
    PN_CPP_EXTERN static start list() { return start(LIST); }
    PN_CPP_EXTERN static start map() { return start(MAP); }
    PN_CPP_EXTERN static start described() { return start(DESCRIBED, NULL_TYPE, true); }
};

// Finish inserting or extracting a complex type.
struct finish {};

} // codec

} // proton

#endif  /*!PROTON_DATA_HPP*/
