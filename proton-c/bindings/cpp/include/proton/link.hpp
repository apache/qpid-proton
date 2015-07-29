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
#include "proton/export.hpp"
#include "proton/proton_handle.hpp"
#include "proton/endpoint.hpp"
#include "proton/terminus.hpp"
#include "proton/types.h"
#include <string>

struct pn_connection_t;

namespace proton {

/** Messages are transferred across a link. Base class for sender, receiver. */
class link : public endpoint, public proton_handle<pn_link_t>
{
  public:
    PN_CPP_EXTERN link(pn_link_t *);
    PN_CPP_EXTERN link();
    PN_CPP_EXTERN ~link();
    PN_CPP_EXTERN link(const link&);
    PN_CPP_EXTERN link& operator=(const link&);

    /** Locally open the link, not complete till messaging_handler::on_link_opened or
     * proton_handler::link_remote_open
     */
    PN_CPP_EXTERN void open();

    /** Locally close the link, not complete till messaging_handler::on_link_closeed or
     * proton_handler::link_remote_close
     */
    PN_CPP_EXTERN void close();

    /** True if link is a sender */
    PN_CPP_EXTERN bool is_sender();
    /** True if link is a receiver */
    PN_CPP_EXTERN bool is_receiver();
    /** Credit available on the link */
    PN_CPP_EXTERN int credit();
    /** Local source of the link */
    PN_CPP_EXTERN terminus source();
    /** Local target of the link */
    PN_CPP_EXTERN terminus target();
    /** Remote source of the link */
    PN_CPP_EXTERN terminus remote_source();
    /** Remote target of the link */
    PN_CPP_EXTERN terminus remote_target();
    /** Link name */
    PN_CPP_EXTERN std::string name();

    /** Next link that matches state mask. @see endpoint::state */
    PN_CPP_EXTERN link next(endpoint::state mask);

    /** Connection of the link */
    PN_CPP_EXTERN class connection &connection();

    PN_CPP_EXTERN pn_link_t *pn_link() const;

  protected:
    PN_CPP_EXTERN virtual void verify_type(pn_link_t *l);
  private:
    friend class proton_impl_ref<link>;
    bool sender_link;
};

}

#include "proton/sender.hpp"
#include "proton/receiver.hpp"

#endif  /*!PROTON_CPP_LINK_H*/
