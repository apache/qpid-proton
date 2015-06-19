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
#include "proton/export.hpp"
#include "proton/proton_handle.hpp"
#include "proton/url.h"
#include <string>

namespace proton {

class Url : public proton_handle<pn_url_t>
{
  public:
    PN_CPP_EXTERN Url(const std::string &url);
    PN_CPP_EXTERN ~Url();
    PN_CPP_EXTERN Url(const Url&);
    PN_CPP_EXTERN Url& operator=(const Url&);
    PN_CPP_EXTERN std::string host();
    PN_CPP_EXTERN std::string port();
    PN_CPP_EXTERN std::string path();
  private:
    friend class proton_impl_ref<Url>;
};


}

#endif  /*!PROTON_CPP_URL_H*/
