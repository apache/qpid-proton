#ifndef PROTON_IO_DRIVER_HPP
#define PROTON_IO_DRIVER_HPP

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

#include <functional>
#include <memory>

namespace proton {

class controller;

namespace io {

/// A proton::controller implementation can create a static instance of default_controller
/// to register as the default implementation.
/// If more than one implementation is linked, which one becomes the default
/// is undefined.
struct default_controller {

    /// A controller make-function takes a string container-id and returns a unique_ptr<controller>
    typedef std::function<std::unique_ptr<controller>()> make_fn;

    /// Construct a static instance of default_controller to register your controller factory.
    PN_CPP_EXTERN default_controller(make_fn);
};

}}

#endif // PROTON_IO_CONTROLLER_HPP
