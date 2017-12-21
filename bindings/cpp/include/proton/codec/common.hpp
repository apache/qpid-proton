#ifndef PROTON_CODEC_COMMON_HPP
#define PROTON_CODEC_COMMON_HPP

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

#include "../type_id.hpp"

/// @file
/// **Unsettled API** - Shared codec functions.

namespace proton {
namespace codec {

/// **Unsettled API** - Start encoding a complex type.
struct start {
    /// @cond INTERNAL
    /// XXX Document
    start(type_id type_=NULL_TYPE, type_id element_=NULL_TYPE,
          bool described_=false, size_t size_=0) :
        type(type_), element(element_), is_described(described_), size(size_) {}

    type_id type;            ///< The container type: ARRAY, LIST, MAP or DESCRIBED.
    type_id element;         ///< the element type for array only.
    bool is_described;       ///< true if first value is a descriptor.
    size_t size;             ///< the element count excluding the descriptor (if any)
    /// @endcond

    /// @cond INTERNAL
    /// XXX Document
    static start array(type_id element, bool described=false) { return start(ARRAY, element, described); }
    static start list() { return start(LIST); }
    static start map() { return start(MAP); }
    static start described() { return start(DESCRIBED, NULL_TYPE, true); }
    /// @endcond
};

/// **Unsettled API** - Finish inserting or extracting a complex type.
struct finish {};

} // codec
} // proton

#endif // PROTON_CODEC_COMMON_HPP
