#ifndef PROTON_NAMESPACES_HPP
#define PROTON_NAMESPACES_HPP

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

/// The main Proton namespace.
namespace proton {

/// **Experimental** - AMQP data encoding and decoding.
///
/// You can use these classes on an experimental basis to create your
/// own AMQP encodings for C++ types, but they may change in the
/// future. For examples of use see the built-in encodings, for
/// example in proton/vector.hpp or proton/map.hpp
namespace codec {
}

/// **Experimental** - An SPI for multithreaded network IO.
namespace io {
}

namespace internal {
}

} // proton

#endif // PROTON_NAMESPACES_HPP
