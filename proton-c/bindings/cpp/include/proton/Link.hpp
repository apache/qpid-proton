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
#include "proton/ImportExport.hpp"
#include "proton/ProtonHandle.hpp"
#include "proton/Endpoint.hpp"
#include "proton/Terminus.hpp"
#include "proton/types.h"
#include <string>

struct pn_connection_t;

namespace proton {
namespace reactor {

class Link : public Endpoint, public ProtonHandle<pn_link_t>
{
  public:
    PN_CPP_EXTERN Link(pn_link_t *);
    PN_CPP_EXTERN Link();
    PN_CPP_EXTERN ~Link();
    PN_CPP_EXTERN Link(const Link&);
    PN_CPP_EXTERN Link& operator=(const Link&);
    PN_CPP_EXTERN void open();
    PN_CPP_EXTERN void close();
    PN_CPP_EXTERN bool isSender();
    PN_CPP_EXTERN bool isReceiver();
    PN_CPP_EXTERN int getCredit();
    PN_CPP_EXTERN Terminus getSource();
    PN_CPP_EXTERN Terminus getTarget();
    PN_CPP_EXTERN Terminus getRemoteSource();
    PN_CPP_EXTERN Terminus getRemoteTarget();
    PN_CPP_EXTERN std::string getName();
    PN_CPP_EXTERN pn_link_t *getPnLink() const;
    virtual PN_CPP_EXTERN Connection &getConnection();
    PN_CPP_EXTERN Link getNext(Endpoint::State mask);
  protected:
    virtual void verifyType(pn_link_t *l);
  private:
    friend class ProtonImplRef<Link>;
    bool senderLink;
};

}}

#include "proton/Sender.hpp"
#include "proton/Receiver.hpp"

#endif  /*!PROTON_CPP_LINK_H*/
