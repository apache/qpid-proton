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

#include "proton/timestamp.hpp"

#include "proton/internal/config.hpp"
#include <proton/types.h>

#include <iostream>

#if PN_CPP_HAS_CHRONO
#include <chrono>
#else
#include <time.h>
#endif

namespace proton {

#if PN_CPP_HAS_CHRONO
timestamp timestamp::now() {
    using namespace std::chrono;
    return timestamp( duration_cast<std::chrono::milliseconds>(system_clock::now().time_since_epoch()).count() );
}
#else
// Fallback with low (seconds) precision
timestamp timestamp::now() {
    return timestamp( time(0)*1000 );
}
#endif

std::ostream& operator<<(std::ostream& o, timestamp ts) { return o << ts.milliseconds(); }

}
