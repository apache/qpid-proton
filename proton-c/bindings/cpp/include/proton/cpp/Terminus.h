#ifndef PROTON_CPP_TERMINUS_H
#define PROTON_CPP_TERMINUS_H

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
#include "proton/cpp/Link.h"

#include "proton/link.h"
#include <string>

namespace proton {
namespace reactor {

class Link;

class Terminus : public ProtonHandle<pn_terminus_t>
{
    enum Type {
        TYPE_UNSPECIFIED = PN_UNSPECIFIED,
        SOURCE = PN_SOURCE,
        TARGET = PN_TARGET,
        COORDINATOR = PN_COORDINATOR
    };
    enum ExpiryPolicy {
        NONDURABLE = PN_NONDURABLE,
        CONFIGURATION = PN_CONFIGURATION,
        DELIVERIES = PN_DELIVERIES
    };
    enum DistributionMode {
        MODE_UNSPECIFIED = PN_DIST_MODE_UNSPECIFIED,
        COPY = PN_DIST_MODE_COPY,
        MOVE = PN_DIST_MODE_MOVE
    };

  public:
    PROTON_CPP_EXTERN Terminus();
    PROTON_CPP_EXTERN ~Terminus();
    PROTON_CPP_EXTERN Terminus(const Terminus&);
    PROTON_CPP_EXTERN Terminus& operator=(const Terminus&);
    PROTON_CPP_EXTERN pn_terminus_t *getPnTerminus();
    PROTON_CPP_EXTERN Type getType();
    PROTON_CPP_EXTERN void setType(Type);
    PROTON_CPP_EXTERN ExpiryPolicy getExpiryPolicy();
    PROTON_CPP_EXTERN void setExpiryPolicy(ExpiryPolicy);
    PROTON_CPP_EXTERN DistributionMode getDistributionMode();
    PROTON_CPP_EXTERN void setDistributionMode(DistributionMode);
    PROTON_CPP_EXTERN std::string getAddress();
    PROTON_CPP_EXTERN void setAddress(std::string &);
    PROTON_CPP_EXTERN bool isDynamic();
    PROTON_CPP_EXTERN void setDynamic(bool);

  private:
    Link *link;
    PROTON_CPP_EXTERN Terminus(pn_terminus_t *, Link *);
    friend class Link;
    friend class ProtonImplRef<Terminus>;
};


}} // namespace proton::reactor

#endif  /*!PROTON_CPP_TERMINUS_H*/
