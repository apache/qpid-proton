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

#include "proton/sasl.hpp"

namespace proton {

enum sasl::outcome sasl::outcome() const { return static_cast<enum outcome>(pn_sasl_outcome(object_)); }

std::string sasl::user() const {
    const char *name = pn_sasl_get_user(object_);
    return name ? std::string(name) : std::string();
}

std::string sasl::mech() const {
    const char *m = pn_sasl_get_mech(object_);
    return m ? std::string(m) : std::string();
}

void sasl::allow_insecure_mechs(bool allowed) { pn_sasl_set_allow_insecure_mechs(object_, allowed); }
bool sasl::allow_insecure_mechs() { return pn_sasl_get_allow_insecure_mechs(object_); }
void sasl::allowed_mechs(const std::string &mechs) { pn_sasl_allowed_mechs(object_, mechs.c_str()); }
void sasl::config_name(const std::string &name) { pn_sasl_config_name(object_, name.c_str()); }
void sasl::config_path(const std::string &path) { pn_sasl_config_path(object_, path.c_str()); }


} // namespace
