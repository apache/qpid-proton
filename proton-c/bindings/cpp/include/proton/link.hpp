#ifndef PROTON_CPP_LINK_H
#define PROTON_CPP_LINK_H

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
#include "proton/endpoint.hpp"
#include "proton/export.hpp"
#include "proton/message.hpp"
#include "proton/terminus.hpp"
#include "proton/types.h"
#include "proton/facade.hpp"

#include <string>

namespace proton {

class sender;
class receiver;

/** Messages are transferred across a link. Base class for sender, receiver. */
class link : public counted_facade<pn_link_t, link, endpoint>
{
  public:
    /** Locally open the link, not complete till messaging_handler::on_link_opened or
     * proton_handler::link_remote_open
     */
    PN_CPP_EXTERN void open();

    /** Locally close the link, not complete till messaging_handler::on_link_closed or
     * proton_handler::link_remote_close
     */
    PN_CPP_EXTERN void close();

    /** Return sender if this link is a sender, 0 if not. */
    PN_CPP_EXTERN class sender* sender();
    /** Return receiver if this link is a receiver, 0 if not. */
    PN_CPP_EXTERN class receiver* receiver();
    /** Credit available on the link */
    PN_CPP_EXTERN int credit();

    /** True if link has source */
    PN_CPP_EXTERN bool has_source();
    /** True if link has target */
    PN_CPP_EXTERN bool has_target();
    /** True if link has remote source */
    PN_CPP_EXTERN bool has_remote_source();
    /** True if link has remote target */
    PN_CPP_EXTERN bool has_remote_target();

    /** Local source of the link. @throw error if !has_source() */
    PN_CPP_EXTERN terminus& source();
    /** Local target of the link. @throw error if !has_target() */
    PN_CPP_EXTERN terminus& target();
    /** Remote source of the link. @throw error if !has_remote_source() */
    PN_CPP_EXTERN terminus& remote_source();
    /** Remote target of the link. @throw error if !has_remote_target() */
    PN_CPP_EXTERN terminus& remote_target();

    /** Link name */
    PN_CPP_EXTERN std::string name();

    /** Next link that matches state mask. @see endpoint::state.
     * @return 0 if none, do not delete returned pointer
     */
    PN_CPP_EXTERN link* next(endpoint::state mask);

    /** Connection that owns this link */
    PN_CPP_EXTERN class connection &connection();

    /** Set a custom handler for this link. */
    PN_CPP_EXTERN void handler(class handler &);

    /** Unset any custom handler */
    PN_CPP_EXTERN void detach_handler();

    /** Get the endpoint state */
    PN_CPP_EXTERN endpoint::state state();
};

}

#include "proton/sender.hpp"
#include "proton/receiver.hpp"

#endif  /*!PROTON_CPP_LINK_H*/
