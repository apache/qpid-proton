#ifndef PROTON_CPP_SESSION_H
#define PROTON_CPP_SESSION_H

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
#include "proton/link.h"
#include <string>

struct pn_connection_t;
struct pn_session_t;

namespace proton {

class container;
class handler;

/// A container of links.
class session : public object<pn_session_t>, public endpoint
{
  public:
    /// @cond INTERNAL
    session(pn_session_t* s=0) : object<pn_session_t>(s) {}
    /// @endcond

    // Endpoint behaviours

    /// Get the state of this session.
    PN_CPP_EXTERN endpoint::state state() const;
    
    PN_CPP_EXTERN condition local_condition() const;
    PN_CPP_EXTERN condition remote_condition() const;

    /// @cond INTERNAL
    /// XXX needs to take connection options
    /// Initiate local open.  The operation is not complete till
    /// handler::on_session_open().
    PN_CPP_EXTERN void open();
    /// @endcond
    
    /// Initiate local close.  The operation is not complete till
    /// handler::on_session_close().
    PN_CPP_EXTERN void close();

    /// Get the connection this session belongs to.
    PN_CPP_EXTERN class connection connection() const;

    /// @cond INTERNAL
    /// XXX consider removing
    
    /// An unopened receiver link, you can set link properties before calling open().
    ///
    /// @param name if specified must be unique, by default the
    /// container generates a name of the form: <hex-digits> + "@" +
    /// container.id()
    PN_CPP_EXTERN receiver create_receiver(const std::string& name="");

    /// An unopened sender link, you can set link properties before calling open().
    ///
    /// @param name if specified must be unique, by default the
    /// container generates a name of the form: <hex-digits> + "@" +
    /// container.id()
    PN_CPP_EXTERN sender create_sender(const std::string& name="");

    /// @endcond
    
    /// Open a sender for `addr`.
    PN_CPP_EXTERN sender open_sender(const std::string &addr, const link_options &opts = link_options());

    /// Open a receiver for `addr`.
    PN_CPP_EXTERN receiver open_receiver(const std::string &addr, const link_options &opts = link_options());

    /// @cond INTERNAL
    /// XXX is this or should this be obviated by find functions?
    /// Navigate the sessions in a connection - get next session with endpoint state
    PN_CPP_EXTERN session next(endpoint::state) const;
    /// @endcond

    /// Return the links on this session matching the state mask.
    PN_CPP_EXTERN link_range find_links(endpoint::state mask) const;
};

/// @cond INTERNAL
/// XXX should be exposed?
/// An iterator for sessions.
class session_iterator : public iter_base<session> {
 public:
    explicit session_iterator(session p = session(), endpoint::state s = 0) :
        iter_base<session>(p, s) {}
    PN_CPP_EXTERN session_iterator operator++();
    session_iterator operator++(int) { session_iterator x(*this); ++(*this); return x; }
};
/// @endcond
    
/// A range of sessions.
typedef range<session_iterator> session_range;

}

#endif // PROTON_CPP_SESSION_H
