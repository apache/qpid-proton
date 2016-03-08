#ifndef PROTON_TYPES_HPP
#define PROTON_TYPES_HPP
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

///@file
///
/// Include the definitions of all proton types used to represent AMQP types.
/// Provided for convenience, you can include types individually instead.

#include <proton/binary.hpp>
#include <proton/config.hpp>
#include <proton/decimal.hpp>
#include <proton/deque.hpp>
#include <proton/duration.hpp>
#include <proton/list.hpp>
#include <proton/map.hpp>
#include <proton/scalar.hpp>
#include <proton/symbol.hpp>
#include <proton/timestamp.hpp>
#include <proton/types_fwd.hpp>
#include <proton/uuid.hpp>
#include <proton/value.hpp>
#include <proton/vector.hpp>

#include <proton/annotation_key.hpp>
#include <proton/message_id.hpp>

#include <proton/config.hpp>
#if PN_CPP_HAS_CPP11
#include <proton/forward_list.hpp>
#include <proton/unordered_map.hpp>
#endif

#endif // PROTON_TYPES_HPP
