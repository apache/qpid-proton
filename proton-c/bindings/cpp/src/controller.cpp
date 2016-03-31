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

#include "contexts.hpp"

#include <proton/error.hpp>
#include <proton/controller.hpp>
#include <proton/work_queue.hpp>

#include <proton/io/default_controller.hpp>

#include <utility>
#include <memory>

static proton::io::default_controller::make_fn make_default_controller;

namespace proton {

std::unique_ptr<controller> controller::create() {
    if (!make_default_controller)
        throw error("no default controller");
    return make_default_controller();
}

controller& controller::get(const proton::connection& c) {
    return work_queue::get(c)->controller();
}

std::shared_ptr<work_queue> work_queue::get(const proton::connection& c) {
    work_queue* wq = connection_context::get(c).work_queue;
    if (!wq)
        throw proton::error("connection has no controller");
    return wq->shared_from_this();
}

namespace io {
// Register a default controller factory.
default_controller::default_controller(default_controller::make_fn f) {
    make_default_controller = f;
}
} // namespace io

} // namespace proton
