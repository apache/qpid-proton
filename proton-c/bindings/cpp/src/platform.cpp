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

#include "platform.h"
#include <string>

// Copy neccesary platform neutral functionality from Proton-C
// TODO: make this sensibly maintainable (even though it is mostly static)

#ifdef USE_UUID_GENERATE
#include <uuid/uuid.h>
#include <stdlib.h>
char* pn_i_genuuid(void) {
    char *generated = (char *) malloc(37*sizeof(char));
    uuid_t uuid;
    uuid_generate(uuid);
    uuid_unparse(uuid, generated);
    return generated;
}
#elif USE_UUID_CREATE
#include <uuid.h>
char* pn_i_genuuid(void) {
    char *generated;
    uuid_t uuid;
    uint32_t rc;
    uuid_create(&uuid, &rc);
    // Under FreeBSD the returned string is newly allocated from the heap
    uuid_to_string(&uuid, &generated, &rc);
    return generated;
}
#elif USE_WIN_UUID
#include <rpc.h>
char* pn_i_genuuid(void) {
    unsigned char *generated;
    UUID uuid;
    UuidCreate(&uuid);
    UuidToString(&uuid, &generated);
    char* r = pn_strdup((const char*)generated);
    RpcStringFree(&generated);
    return r;
}
#else
#error "Don't know how to generate uuid strings on this platform"
#endif



namespace proton {
namespace reactor {

// include Proton-c platform routines into a local namespace


std::string generateUuid() {
    char *s = pn_i_genuuid();
    std::string url(s);
    free(s);
    return url;
}

}} // namespace proton::reactor
