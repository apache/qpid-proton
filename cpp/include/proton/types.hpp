#ifndef PROTON_TYPES_HPP
#define PROTON_TYPES_HPP

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

/// @file
/// Proton types used to represent AMQP types.

// TODO aconway 2016-03-15: described types, described arrays.

#include "./internal/config.hpp"

#include "./annotation_key.hpp"
#include "./binary.hpp"
#include "./decimal.hpp"
#include "./duration.hpp"
#include "./message_id.hpp"
#include "./null.hpp"
#include "./scalar.hpp"
#include "./symbol.hpp"
#include "./timestamp.hpp"
#include "./uuid.hpp"
#include "./value.hpp"

#include "./codec/deque.hpp"
#include "./codec/list.hpp"
#include "./codec/map.hpp"
#include "./codec/vector.hpp"
#if PN_CPP_HAS_CPP11
#include "./codec/forward_list.hpp"
#include "./codec/unordered_map.hpp"
#endif

#endif // PROTON_TYPES_HPP
