#ifndef PROTON_CPP_URL_H
#define PROTON_CPP_URL_H

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
#include "proton/url.h"
#include <string>

namespace proton {
namespace reactor {

class Url : public ProtonHandle<pn_url_t>
{
  public:
    PROTON_CPP_EXTERN Url(const std::string &url);
    PROTON_CPP_EXTERN ~Url();
    PROTON_CPP_EXTERN Url(const Url&);
    PROTON_CPP_EXTERN Url& operator=(const Url&);
    PROTON_CPP_EXTERN std::string getHost();
    PROTON_CPP_EXTERN std::string getPort();
    PROTON_CPP_EXTERN std::string getPath();
  private:
    friend class ProtonImplRef<Url>;
};


}} // namespace proton::reactor

#endif  /*!PROTON_CPP_URL_H*/
