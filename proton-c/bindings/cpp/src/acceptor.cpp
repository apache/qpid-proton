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

#include "proton/acceptor.hpp"
#include "proton/error.hpp"
#include "proton_impl_ref.hpp"
#include "msg.hpp"

namespace proton {

template class proton_handle<pn_acceptor_t>;
typedef proton_impl_ref<acceptor> PI;

acceptor::acceptor() {}

acceptor::acceptor(pn_acceptor_t *a)
{
    PI::ctor(*this, a);
}

acceptor::~acceptor() { PI::dtor(*this); }


acceptor::acceptor(const acceptor& a) : proton_handle<pn_acceptor_t>() {
    PI::copy(*this, a);
}

acceptor& acceptor::operator=(const acceptor& a) {
    return PI::assign(*this, a);
}

void acceptor::close() {
    if (impl_)
        pn_acceptor_close(impl_);
}

}
