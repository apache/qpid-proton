#ifndef PROTON_TRANSACTION_OPTIONS_HPP
#define PROTON_TRANSACTION_OPTIONS_HPP

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

#include <memory>

/// @file
/// @copybrief proton::transaction_options

namespace proton {

/// Options for declaring a session transaction.
///
/// Options can be "chained" (see proton::connection_options).
///
/// Normal value semantics: copy or assign creates a separate copy of
/// the options.
class transaction_options {
  public:
    /// Create a set of options (unset fields use their documented defaults on declare).
    PN_CPP_EXTERN transaction_options();

    /// Copy options.
    PN_CPP_EXTERN transaction_options(const transaction_options&);

    PN_CPP_EXTERN ~transaction_options();

    /// Copy options.
    PN_CPP_EXTERN transaction_options& operator=(const transaction_options&);

    /// Merge with another option set; values from \p other take precedence.
    PN_CPP_EXTERN void update(const transaction_options& other);

    /// **Unsettled API** — If true, when a transaction is aborted, or a commit
    /// discharge is rejected, the client automatically dispositions any unsettled
    /// incoming deliveries (on receiver links) in this session with a modified (failed)
    /// outcome. If false, the application must update those deliveries itself.
    /// The default is true.
    PN_CPP_EXTERN transaction_options& auto_modify_on_abort(bool);

  private:
    void apply(class session&) const;

    class impl;
    std::unique_ptr<impl> impl_;

  /// @cond INTERNAL
  friend class session;
  /// @endcond
};

} // namespace proton

#endif // PROTON_TRANSACTION_OPTIONS_HPP
