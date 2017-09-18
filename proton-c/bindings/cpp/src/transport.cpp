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

#include "proton/error.hpp"
#include "proton/transport.hpp"
#include "proton/error_condition.hpp"
#include "proton/connection.hpp"
#include "proton/ssl.hpp"
#include "proton/sasl.hpp"
#include <proton/transport.h>
#include <proton/error.h>

#include "msg.hpp"
#include "proton_bits.hpp"


namespace proton {

connection transport::connection() const {
    return make_wrapper(pn_transport_connection(pn_object()));
}

class ssl transport::ssl() const {
    return pn_ssl(pn_object());
}

class sasl transport::sasl() const {
    return pn_sasl(pn_object());
}

error_condition transport::error() const {
    return make_wrapper(pn_transport_condition(pn_object()));
}

}
