#ifndef PROTON_SENDER_OPTIONS_HPP
#define PROTON_SENDER_OPTIONS_HPP

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
class sender;
class source_options;
class target_options;

/// Options for creating a sender.
///
/// Options can be "chained" like this:
///
/// @code
/// l = container.create_sender(url, sender_options().handler(h).auto_settle(false));
/// @endcode
///
/// You can also create an options object with common settings and use
/// it as a base for different connections that have mostly the same
/// settings:
///
/// @code
/// sender_options opts;
/// opts.browsing(true);
/// l1 = container.open_sender(url1, opts.handler(h1));
/// c2 = container.open_receiver(url2, opts.handler(h2));
/// @endcode
///
/// Normal value semantics: copy or assign creates a separate copy of
/// the options.
// XXX opts.browsing is not a good example here
class sender_options {
  public:
    /// Create an empty set of options.
    PN_CPP_EXTERN sender_options();

    /// Copy options.
    PN_CPP_EXTERN sender_options(const sender_options&);

    PN_CPP_EXTERN ~sender_options();

    /// Copy options.
    PN_CPP_EXTERN sender_options& operator=(const sender_options&);

    /// Merge with another option set
    PN_CPP_EXTERN void update(const sender_options& other);

    /// Set a messaging_handler for sender events only.
    /// The handler is no longer in use when messaging_handler::on_sender_close() is called.
    /// messaging_handler::on_sender_close() may not be called if a connection is aborted,
    /// in that case it should be cleaned up in its connection's messaging_handler::on_transport_close()
    PN_CPP_EXTERN sender_options& handler(class messaging_handler&);

    /// Set the delivery mode on the sender.
    PN_CPP_EXTERN sender_options& delivery_mode(delivery_mode);

    /// Automatically settle messages (default is true).
    PN_CPP_EXTERN sender_options& auto_settle(bool);

    /// Options for the source node of the sender.
    PN_CPP_EXTERN sender_options& source(source_options &);

    /// Options for the receiver node of the receiver.
    PN_CPP_EXTERN sender_options& target(target_options &);

    /// @cond INTERNAL
  private:
    void apply(sender&) const;

    class impl;
    internal::pn_unique_ptr<impl> impl_;

    friend class sender;
    /// @endcond
};

} // proton

#endif // PROTON_SENDER_OPTIONS_HPP
