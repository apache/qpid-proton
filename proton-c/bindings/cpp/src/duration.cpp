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
#include "proton/duration.hpp"
#include "proton/timestamp.hpp"

#include <limits>
#include <iostream>

namespace proton {

const duration duration::FOREVER(std::numeric_limits<duration::numeric_type>::max());
const duration duration::IMMEDIATE(0);
const duration duration::MILLISECOND(1);
const duration duration::SECOND(1000);
const duration duration::MINUTE(SECOND * 60);

std::ostream& operator<<(std::ostream& o, duration d) { return o << d.milliseconds(); }

}
