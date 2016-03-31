#ifndef PROTON_WORK_QUEUE_HPP
#define PROTON_WORK_QUEUE_HPP

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
pp * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <proton/handler.hpp>
#include <proton/connection_options.hpp>

#include <functional>
#include <memory>

namespace proton {

class connection;

/// A work_queue takes work (in the form of function objects) that will be be
/// serialized with other activity on a connection. Typically the work is a call
/// to user-defined member functions on the handler(s) associated with a
/// connection, which will be called serialized with respect to
/// proton::handler::on_* event functions.
///
class work_queue : public std::enable_shared_from_this<work_queue> {
  public:
    work_queue(const work_queue&) = delete;
    virtual ~work_queue() {}

    /// Get the work_queue associated with a connection.
    /// @throw proton::error if this is not a controller-managed connection.
    PN_CPP_EXTERN static std::shared_ptr<work_queue> get(const proton::connection&);

    /// push a function object on the queue to be invoked in a safely serialized
    /// away.
    ///
    /// @return true if `f()` was pushed and will be called. False if the
    /// work_queue is already closed and f() will never be called.
    ///
    /// Note 1: On returning true, the application can rely on f() being called
    /// eventually. However f() should check the state when it executes as
    /// links, sessions or even the connection may have closed by the time f()
    /// is executed.
    ///
    /// Note 2: You must not push() in a handler or work_queue function on the
    /// *same connection* as the work_queue you are pushing to. That could cause
    /// a deadlock.
    ///
    virtual bool push(std::function<void()>) = 0;

    /// Get the controller associated with this work_queue.
    virtual class controller& controller() const = 0;

  protected:
    work_queue() {}
};

}


#endif // PROTON_WORK_QUEUE_HPP
