#ifndef PROTON_RECONNECT_OPTIONS_HPP
#define PROTON_RECONNECT_OPTIONS_HPP

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

#include "./internal/export.hpp"
#include "./internal/pn_unique_ptr.hpp"
#include "./duration.hpp"
#include "./source.hpp"

#include <string>
#include <vector>

namespace proton {

/// **Experimental** - Options that determine a series of delays to
/// coordinate reconnection attempts.  They may be open ended or
/// limited in time.  They may be evenly spaced or increasing at an
/// exponential rate.
///
/// Options can be "chained" (@see proton::connection_options).
///
/// Normal value semantics: copy or assign creates a separate copy of
/// the options.
class reconnect_options {
  public:

    /// Create an empty set of options.
    PN_CPP_EXTERN reconnect_options();

    /// Copy options.
    PN_CPP_EXTERN reconnect_options(const reconnect_options&);

    PN_CPP_EXTERN ~reconnect_options();

    /// Copy options.
    PN_CPP_EXTERN reconnect_options& operator=(const reconnect_options&);

    /// Base value for recurring delay (default is 10 milliseconds).
    PN_CPP_EXTERN reconnect_options& delay(duration);

    /// Scaling multiplier for successive reconnect delays (default is 2.0)
    PN_CPP_EXTERN reconnect_options& delay_multiplier(float);

    /// Maximum delay between successive connect attempts (default
    /// duration::FOREVER, i.e. no limit)
    PN_CPP_EXTERN reconnect_options& max_delay(duration);

    /// Maximum reconnect attempts (default 0, meaning no limit)
    PN_CPP_EXTERN reconnect_options& max_attempts(int);

    /// Alternative connection urls used for failover
    PN_CPP_EXTERN reconnect_options& failover_urls(const std::vector<std::string>& urls);

  private:
    class impl;
    internal::pn_unique_ptr<impl> impl_;

    /// @cond INTERNAL
  friend class container;
      /// @endcond
};

} // proton

#endif // PROTON_RECONNECT_OPTIONS_HPP
