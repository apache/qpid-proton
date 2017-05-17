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

#include "sasl-internal.h"

#include "core/engine-internal.h"
#include "core/util.h"
#include "proton/proactor.h"

typedef struct
{
    size_t size;
    char *start;
} pni_owned_bytes_t;

const int8_t UPSTREAM_INIT_RECEIVED = 1;
const int8_t UPSTREAM_RESPONSE_RECEIVED = 2;
const int8_t DOWNSTREAM_MECHANISMS_RECEIVED = 3;
const int8_t DOWNSTREAM_CHALLENGE_RECEIVED = 4;
const int8_t DOWNSTREAM_OUTCOME_RECEIVED = 5;

const char* authentication_service_address = NULL;

typedef struct
{
    pn_connection_t* downstream;
    char* selected_mechanism;
    pni_owned_bytes_t response;
    int8_t downstream_state;

    pn_connection_t* upstream;
    char* mechlist;
    pni_owned_bytes_t challenge;
    int8_t upstream_state;

    pn_sasl_outcome_t outcome;
    int refcount;
} pni_sasl_relay_t;

void pni_copy_bytes(const pn_bytes_t* from, pni_owned_bytes_t* to)
{
    if (to->start) {
        free(to->start);
    }
    to->start = (char*) malloc(from->size);
    to->size = from->size;
    memcpy(to->start, from->start, from->size);
}

pni_sasl_relay_t* new_pni_sasl_relay_t(void)
{
    pni_sasl_relay_t* instance = (pni_sasl_relay_t*) malloc(sizeof(pni_sasl_relay_t));
    instance->selected_mechanism = 0;
    instance->response.start = 0;
    instance->response.size = 0;
    instance->mechlist = 0;
    instance->challenge.start = 0;
    instance->challenge.size = 0;
    instance->refcount = 1;
    instance->upstream_state = 0;
    instance->downstream_state = 0;
    return instance;
}

void delete_pni_sasl_relay_t(pni_sasl_relay_t* instance)
{
    if (instance->mechlist) free(instance->mechlist);
    if (instance->selected_mechanism) free(instance->selected_mechanism);
    if (instance->response.start) free(instance->response.start);
    if (instance->challenge.start) free(instance->challenge.start);
    free(instance);
}

void release_pni_sasl_relay_t(pni_sasl_relay_t* instance)
{
    if (instance && --(instance->refcount) == 0) {
        delete_pni_sasl_relay_t(instance);
    }
}

bool remote_init_server(pn_transport_t* transport)
{
    pn_connection_t* upstream = pn_transport_connection(transport);
    if (upstream) {
        if (transport->sasl->impl_context) {
            return true;
        }
        pni_sasl_relay_t* impl = new_pni_sasl_relay_t();
        transport->sasl->impl_context = impl;
        impl->upstream = upstream;
        pn_proactor_t* proactor = pn_connection_proactor(upstream);
        if (!proactor) return false;
        impl->downstream = pn_connection();
        pn_connection_set_hostname(impl->downstream, pn_connection_get_hostname(upstream));
        //do I need to explicitly set up sasl? if so how? need to handle connection_bound?
        //for now just fake it with dummy user
        pn_connection_set_user(impl->downstream, "dummy");
        pn_connection_set_context(impl->downstream, transport->sasl->impl_context);//TODO: use record?

        pn_proactor_connect(proactor, impl->downstream, authentication_service_address);
        return true;
    } else {
        return false;
    }
}

bool remote_init_client(pn_transport_t* transport)
{
    //for the client side of the connection to the authentication
    //service, need to use the same context as the server side of the
    //connection it is authenticating on behalf of
    pn_connection_t* conn = pn_transport_connection(transport);
    transport->sasl->impl_context = pn_connection_get_context(conn);
    ((pni_sasl_relay_t*) transport->sasl->impl_context)->refcount++;
    return true;
}

bool remote_free(pn_transport_t *transport)
{
    if (transport->sasl->impl_context) {
        release_pni_sasl_relay_t((pni_sasl_relay_t*) transport->sasl->impl_context);
        return true;
    } else {
        return false;
    }
}

bool remote_prepare(pn_transport_t *transport)
{
    pni_sasl_relay_t* impl = (pni_sasl_relay_t*) transport->sasl->impl_context;
    if (!impl) return false;
    if (transport->sasl->client) {
        if (impl->downstream_state == UPSTREAM_INIT_RECEIVED) {
            transport->sasl->selected_mechanism = impl->selected_mechanism;
            transport->sasl->bytes_out.start = impl->response.start;
            transport->sasl->bytes_out.size = impl->response.size;
            pni_sasl_set_desired_state(transport, SASL_POSTED_INIT);
        } else if (impl->downstream_state == UPSTREAM_RESPONSE_RECEIVED) {
            transport->sasl->bytes_out.start = impl->response.start;
            transport->sasl->bytes_out.size = impl->response.size;
            pni_sasl_set_desired_state(transport, SASL_POSTED_RESPONSE);
        }
        impl->downstream_state = 0;
    } else {
        if (impl->upstream_state == DOWNSTREAM_MECHANISMS_RECEIVED) {
            pni_sasl_set_desired_state(transport, SASL_POSTED_MECHANISMS);
        } else if (impl->upstream_state == DOWNSTREAM_CHALLENGE_RECEIVED) {
            transport->sasl->bytes_out.start = impl->challenge.start;
            transport->sasl->bytes_out.size = impl->challenge.size;
            pni_sasl_set_desired_state(transport, SASL_POSTED_CHALLENGE);
        } else if (impl->upstream_state == DOWNSTREAM_OUTCOME_RECEIVED) {
            transport->sasl->outcome = impl->outcome;
            pni_sasl_set_desired_state(transport, SASL_POSTED_OUTCOME);
        }
        impl->upstream_state = 0;
    }
    return true;
}

// Client / Downstream
bool remote_process_mechanisms(pn_transport_t *transport, const char *mechs)
{
    pni_sasl_relay_t* impl = (pni_sasl_relay_t*) transport->sasl->impl_context;
    if (impl) {
        if (impl->upstream_state != DOWNSTREAM_MECHANISMS_RECEIVED) {
            impl->mechlist = pn_strdup(mechs);
            impl->upstream_state = DOWNSTREAM_MECHANISMS_RECEIVED;
            pn_connection_wake(impl->upstream);
        }
        return true;
    } else {
        return false;
    }
}

// Client / Downstream
void remote_process_challenge(pn_transport_t *transport, const pn_bytes_t *recv)
{
    pni_sasl_relay_t* impl = (pni_sasl_relay_t*) transport->sasl->impl_context;
    if (impl && impl->upstream_state != DOWNSTREAM_CHALLENGE_RECEIVED) {
        pni_copy_bytes(recv, &(impl->challenge));
        impl->upstream_state = DOWNSTREAM_CHALLENGE_RECEIVED;
        pn_connection_wake(impl->upstream);
    }
}

// Client / Downstream
bool remote_process_outcome(pn_transport_t *transport)
{
    pni_sasl_relay_t* impl = (pni_sasl_relay_t*) transport->sasl->impl_context;
    if (impl) {
        if (impl->upstream_state != DOWNSTREAM_OUTCOME_RECEIVED) {
            impl->outcome = transport->sasl->outcome;
            impl->upstream_state = DOWNSTREAM_OUTCOME_RECEIVED;
            pn_connection_wake(impl->upstream);
        }
        return true;
    } else {
        return false;
    }
}

// Server / Upstream
int remote_get_mechs(pn_transport_t *transport, char **mechlist)
{
    pni_sasl_relay_t* impl = (pni_sasl_relay_t*) transport->sasl->impl_context;
    if (impl && impl->mechlist) {
        *mechlist = pn_strdup(impl->mechlist);
        return 1;
    } else {
        return 0;
    }
}

// Server / Upstream
void remote_process_init(pn_transport_t *transport, const char *mechanism, const pn_bytes_t *recv)
{
    pni_sasl_relay_t* impl = (pni_sasl_relay_t*) transport->sasl->impl_context;
    if (impl) {
        impl->selected_mechanism = pn_strdup(mechanism);
        pni_copy_bytes(recv, &(impl->response));
        impl->downstream_state = UPSTREAM_INIT_RECEIVED;
        pn_connection_wake(impl->downstream);
    }
}

// Server / Upstream
void remote_process_response(pn_transport_t *transport, const pn_bytes_t *recv)
{
    pni_sasl_relay_t* impl = (pni_sasl_relay_t*) transport->sasl->impl_context;
    if (impl) {
        pni_copy_bytes(recv, &(impl->response));
        impl->downstream_state = UPSTREAM_RESPONSE_RECEIVED;
        pn_connection_wake(impl->downstream);
    }
}

void pn_use_remote_authentication_service(const char* address)
{
    authentication_service_address = address;
    pni_sasl_implementation remote_impl;
    remote_impl.free = &remote_free;
    remote_impl.get_mechs = &remote_get_mechs;
    remote_impl.init_server = &remote_init_server;
    remote_impl.process_init = &remote_process_init;
    remote_impl.process_response = &remote_process_response;
    remote_impl.init_client = &remote_init_client;
    remote_impl.process_mechanisms = &remote_process_mechanisms;
    remote_impl.process_challenge = &remote_process_challenge;
    remote_impl.process_outcome = &remote_process_outcome;
    remote_impl.prepare = &remote_prepare;
    pni_sasl_set_implementation(remote_impl);
}
