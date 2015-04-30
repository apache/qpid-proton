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
#include "proton/cpp/ImportExport.h"
#include "proton/cpp/ProtonHandle.h"
#include "proton/cpp/Endpoint.h"
#include "proton/types.h"
#include <string>

struct pn_connection_t;

namespace proton {
namespace reactor {

class Link : public Endpoint, public ProtonHandle<pn_link_t>
{
  public:
    PROTON_CPP_EXTERN Link(pn_link_t *);
    PROTON_CPP_EXTERN Link();
    PROTON_CPP_EXTERN ~Link();
    PROTON_CPP_EXTERN Link(const Link&);
    PROTON_CPP_EXTERN Link& operator=(const Link&);
    PROTON_CPP_EXTERN void open();
    PROTON_CPP_EXTERN void close();
    PROTON_CPP_EXTERN bool isSender();
    PROTON_CPP_EXTERN bool isReceiver();
    PROTON_CPP_EXTERN int getCredit();
    PROTON_CPP_EXTERN pn_link_t *getPnLink() const;
    virtual PROTON_CPP_EXTERN Connection &getConnection();
  protected:
    virtual void verifyType(pn_link_t *l);
  private:
    friend class ProtonImplRef<Link>;
    bool senderLink;
};


}} // namespace proton::reactor

#include "proton/cpp/Sender.h"
#include "proton/cpp/Receiver.h"

#endif  /*!PROTON_CPP_LINK_H*/
