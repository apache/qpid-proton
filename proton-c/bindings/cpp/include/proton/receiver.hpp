#ifndef PROTON_CPP_RECEIVER_H
#define PROTON_CPP_RECEIVER_H

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

#include "proton/export.hpp"
#include "proton/endpoint.hpp"
#include "proton/link.hpp"
#include "proton/types.h"
#include <string>

struct pn_connection_t;

namespace proton {

/// A link for receiving messages.
class
PN_CPP_CLASS_EXTERN receiver : public link {
    /// @cond INTERNAL
    receiver(pn_link_t* r);
    /// @endcond

  public:
    receiver() {}

    /// Locally open the receiver.  The operation is not complete till
    /// handler::on_receiver_open.
    PN_CPP_EXTERN void open(const receiver_options &opts = receiver_options());

    /// Get the source node.
    PN_CPP_EXTERN class source source() const;

    /// Get the target node.
    PN_CPP_EXTERN class target target() const;

    /// @cond INTERNAL
  friend class internal::factory<receiver>;
  friend class receiver_iterator;
  friend class source;
  friend class target;
    /// @endcond
};

class receiver_iterator : public internal::iter_base<receiver, receiver_iterator> {
    ///@cond INTERNAL
    explicit receiver_iterator(receiver r, pn_session_t* s = 0) :
        iter_base<receiver, receiver_iterator>(r), session_(s) {}
    ///@endcond

  public:
    explicit receiver_iterator() :
        iter_base<receiver, receiver_iterator>(0), session_(0) {}
    /// Advance
    PN_CPP_EXTERN receiver_iterator operator++();

  private:
    pn_session_t* session_;

    friend class connection;
    friend class session;
};

/// A range of receivers.
typedef internal::iter_range<receiver_iterator> receiver_range;


}

#endif // PROTON_CPP_RECEIVER_H
