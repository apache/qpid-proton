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

#include "proton/transaction_handler.hpp"
#include "proton/delivery.h"
#include "proton/delivery.hpp"
#include "proton/message.hpp"
#include "proton/target_options.hpp"
#include "proton/tracker.hpp"
#include "proton/transfer.hpp"

#include "proton_bits.hpp"
#include <proton/types.hpp>

#include <iostream>

namespace proton {

transaction_handler::~transaction_handler() = default;
void transaction_handler::on_transaction_declared(session) {}
void transaction_handler::on_transaction_committed(session) {}
void transaction_handler::on_transaction_aborted(session) {}
void transaction_handler::on_transaction_declare_failed(session) {}
void transaction_handler::on_transaction_commit_failed(session) {}

}
