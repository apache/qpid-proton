#ifndef PROTON_RECEIVER_OPTIONS_HPP
#define PROTON_RECEIVER_OPTIONS_HPP

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

#include "./internal/config.hpp"
#include "./internal/export.hpp"
#include "./internal/pn_unique_ptr.hpp"
#include "./types.hpp"
#include "./delivery_mode.hpp"
#include "./terminus.hpp"

#include <vector>
#include <string>

namespace proton {

class proton_handler;
class receiver;
class source_options;
class target_options;

/// Options for creating a receiver.
///
/// Options can be "chained" like this:
///
/// @code
/// l = container.create_receiver(url, receiver_options().handler(h).auto_settle(true));
/// @endcode
///
/// You can also create an options object with common settings and use
/// it as a base for different connections that have mostly the same
/// settings:
///
/// @code
/// receiver_options opts;
/// opts.auto_settle(true);
/// c2 = container.open_receiver(url2, opts.handler(h2));
/// @endcode
///
/// Normal value semantics: copy or assign creates a separate copy of
/// the options.
class receiver_options {
  public:
    /// Create an empty set of options.
    PN_CPP_EXTERN receiver_options();

    /// Copy options.
    PN_CPP_EXTERN receiver_options(const receiver_options&);

    PN_CPP_EXTERN ~receiver_options();

    /// Copy options.
    PN_CPP_EXTERN receiver_options& operator=(const receiver_options&);

    /// Merge with another option set
    PN_CPP_EXTERN void update(const receiver_options& other);

    /// Set a messaging_handler for receiver events only.
    /// The handler is no longer in use when messaging_handler::on_receiver_close() is called.
    PN_CPP_EXTERN receiver_options& handler(class messaging_handler&);

    /// Set the delivery mode on the receiver.
    PN_CPP_EXTERN receiver_options& delivery_mode(delivery_mode);

    /// Automatically accept inbound messages that aren't otherwise
    /// released, rejected, or modified (default is true).
    PN_CPP_EXTERN receiver_options& auto_accept(bool);

    /// Automatically settle messages (default is true).
    PN_CPP_EXTERN receiver_options& auto_settle(bool);

    /// Options for the source node of the receiver.
    PN_CPP_EXTERN receiver_options& source(source_options &);

    /// Options for the target node of the receiver.
    PN_CPP_EXTERN receiver_options& target(target_options &);

    /// Set automated flow control to pre-fetch this many messages
    /// (default is 10).  Set to zero to disable automatic credit
    /// replenishing.
    PN_CPP_EXTERN receiver_options& credit_window(int);

    /// @cond INTERNAL
  private:
    void apply(receiver &) const;

    class impl;
    internal::pn_unique_ptr<impl> impl_;

    friend class receiver;
    /// @endcond
};

} // proton

#endif // PROTON_RECEIVER_OPTIONS_HPP
