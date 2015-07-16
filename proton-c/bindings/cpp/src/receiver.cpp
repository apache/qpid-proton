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
#include "proton/link.hpp"
#include "proton/receiver.hpp"
#include "proton/error.hpp"
#include "msg.hpp"

#include "proton/connection.h"
#include "proton/session.h"
#include "proton/link.h"

namespace proton {


receiver::receiver(pn_link_t *lnk) : link(lnk) {}
receiver::receiver() : link(0) {}

receiver::receiver(const link& c) : link(c.pn_link()) {}

void receiver::verify_type(pn_link_t *lnk) {
    if (lnk && pn_link_is_sender(lnk))
        throw error(MSG("Creating receiver with sender context"));
}


}
