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

#include "proton/ssl.hpp"

struct pn_ssl_domain_t;

namespace proton {

class ssl_options_impl {
  public:
    ssl_options_impl(bool is_server);
    ~ssl_options_impl();

    void incref() {++refcount_;}
    void decref() {if (--refcount_==0) delete this;}
    pn_ssl_domain_t* pn_domain() {return pn_domain_;}

  private:
    pn_ssl_domain_t *pn_domain_;
    int refcount_;
};

class ssl_server_options::impl : public ssl_options_impl {
public:
    impl() : ssl_options_impl(true) {}
};

class ssl_client_options::impl : public ssl_options_impl {
public:
    impl() : ssl_options_impl(false) {}
};

}
