#ifndef PROTON_DEFAULT_CONTAINER_HPP
#define PROTON_DEFAULT_CONTAINER_HPP

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

#include "./container.hpp"

#include "./internal/config.hpp"
#include "./internal/export.hpp"

#include <memory>
#include <string>

namespace proton {
class messaging_handler;

// Avoid deprecated diagnostics from auto_ptr
#if defined(__GNUC__) && __GNUC__*100 + __GNUC_MINOR__ >= 406 || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
/// Default container factory for C++03, not recommended unless you only have C++03
PN_CPP_EXTERN std::auto_ptr<container> make_auto_default_container(messaging_handler&, const std::string& id="");
/// Default container factory for C++03, not recommended unless you only have C++03
PN_CPP_EXTERN std::auto_ptr<container> make_auto_default_container(const std::string& id="");

#if PN_CPP_HAS_UNIQUE_PTR
/// Default container factory
PN_CPP_EXTERN std::unique_ptr<container> make_default_container(messaging_handler&, const std::string& id="");
/// Default container factory
PN_CPP_EXTERN std::unique_ptr<container> make_default_container(const std::string& id="");
#endif

#if PN_CPP_HAS_UNIQUE_PTR
class default_container : public container_ref<std::unique_ptr<container> > {
public:
  default_container(messaging_handler& h, const std::string& id="") : container_ref(make_default_container(h, id)) {}
  default_container(const std::string& id="") : container_ref(make_default_container(id)) {}
};
#else
class default_container : public container_ref<std::auto_ptr<container> > {
public:
  default_container(messaging_handler& h, const std::string& id="") : container_ref<std::auto_ptr<container> >(make_auto_default_container(h, id)) {}
  default_container(const std::string& id="") : container_ref<std::auto_ptr<container> >(make_auto_default_container(id)) {}
};
#endif

#if defined(__GNUC__) && __GNUC__*100 + __GNUC_MINOR__ >= 406 || defined(__clang__)
#pragma GCC diagnostic pop
#endif

} // proton

#endif // PROTON_DEFAULT_CONTAINER_HPP
