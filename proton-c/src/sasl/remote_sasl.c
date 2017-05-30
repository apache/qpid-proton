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
#include "proton/remote_sasl.h"

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
const int8_t DOWNSTREAM_CLOSED = 6;

typedef struct
{
    char* authentication_service_address;

    pn_connection_t* downstream;
    char* selected_mechanism;
    pni_owned_bytes_t response;
    int8_t downstream_state;
    bool downstream_released;

    pn_connection_t* upstream;
    char* mechlist;
    pni_owned_bytes_t challenge;
    int8_t upstream_state;
    bool upstream_released;

    bool complete;
    pn_sasl_outcome_t outcome;
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

pni_sasl_relay_t* new_pni_sasl_relay_t(const char* address)
{
    pni_sasl_relay_t* instance = (pni_sasl_relay_t*) malloc(sizeof(pni_sasl_relay_t));
    instance->authentication_service_address = pn_strdup(address);
    instance->selected_mechanism = 0;
    instance->response.start = 0;
    instance->response.size = 0;
    instance->mechlist = 0;
    instance->challenge.start = 0;
    instance->challenge.size = 0;
    instance->upstream_state = 0;
    instance->downstream_state = 0;
    instance->upstream_released = false;
    instance->downstream_released = false;
    instance->complete = false;
    instance->upstream = 0;
    instance->downstream = 0;
    return instance;
}

void delete_pni_sasl_relay_t(pni_sasl_relay_t* instance)
{
    if (instance->authentication_service_address) free(instance->authentication_service_address);
    if (instance->mechlist) free(instance->mechlist);
    if (instance->selected_mechanism) free(instance->selected_mechanism);
    if (instance->response.start) free(instance->response.start);
    if (instance->challenge.start) free(instance->challenge.start);
    if (instance->downstream) {
        pn_connection_release(instance->downstream);
        instance->downstream = 0;
    }
    free(instance);
}

PN_HANDLE(REMOTE_SASL_CTXT)

bool pn_is_authentication_service_connection(pn_connection_t* conn)
{
    if (conn) {
        pn_record_t *r = pn_connection_attachments(conn);
        return pn_record_has(r, REMOTE_SASL_CTXT);
    } else {
        return false;
    }
}

pni_sasl_relay_t* get_sasl_relay_context(pn_connection_t* conn)
{
    if (conn) {
        pn_record_t *r = pn_connection_attachments(conn);
        if (pn_record_has(r, REMOTE_SASL_CTXT)) {
            return (pni_sasl_relay_t*) pn_record_get(r, REMOTE_SASL_CTXT);
        } else {
            return NULL;
        }
    } else {
        return NULL;
    }
}

void set_sasl_relay_context(pn_connection_t* conn, pni_sasl_relay_t* context)
{
    pn_record_t *r = pn_connection_attachments(conn);
    pn_record_def(r, REMOTE_SASL_CTXT, PN_VOID);
    pn_record_set(r, REMOTE_SASL_CTXT, context);
}

bool remote_init_server(pn_transport_t* transport)
{
    pn_connection_t* upstream = pn_transport_connection(transport);
    if (upstream && transport->sasl->impl_context) {
        pni_sasl_relay_t* impl = (pni_sasl_relay_t*) transport->sasl->impl_context;
        if (impl->upstream) return true;
        impl->upstream = upstream;
        pn_proactor_t* proactor = pn_connection_proactor(upstream);
        if (!proactor) return false;
        impl->downstream = pn_connection();
        pn_connection_set_hostname(impl->downstream, pn_connection_get_hostname(upstream));
        pn_connection_set_user(impl->downstream, "dummy");//force sasl
        set_sasl_relay_context(impl->downstream, impl);

        pn_proactor_connect(proactor, impl->downstream, impl->authentication_service_address);
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
    pni_sasl_relay_t* impl = get_sasl_relay_context(conn);
    if (impl) {
        transport->sasl->impl_context = impl;
        return true;
    } else {
        return false;
    }
}

void remote_free(pn_transport_t *transport)
{
    pni_sasl_relay_t* impl = (pni_sasl_relay_t*) transport->sasl->impl_context;
    if (impl) {
        if (transport->sasl->client) {
            impl->downstream_released = true;
            if (impl->upstream_released) {
                delete_pni_sasl_relay_t(impl);
            } else {
                pn_connection_wake(impl->upstream);
            }
        } else {
            impl->upstream_released = true;
            if (impl->downstream_released) {
                delete_pni_sasl_relay_t(impl);
            } else {
                pn_connection_wake(impl->downstream);
            }
        }
    }
}

bool remote_prepare(pn_transport_t *transport)
{
    pni_sasl_relay_t* impl = (pni_sasl_relay_t*) transport->sasl->impl_context;
    if (!impl) return false;
    if (transport->sasl->client) {
        if (impl->downstream_state == UPSTREAM_INIT_RECEIVED) {
            transport->sasl->selected_mechanism = pn_strdup(impl->selected_mechanism);
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

bool notify_upstream(pni_sasl_relay_t* impl, uint8_t state)
{
    if (!impl->upstream_released) {
        impl->upstream_state = state;
        pn_connection_wake(impl->upstream);
        return true;
    } else {
        return false;
    }
}

bool notify_downstream(pni_sasl_relay_t* impl, uint8_t state)
{
    if (!impl->downstream_released) {
        impl->downstream_state = state;
        pn_connection_wake(impl->downstream);
        return true;
    } else {
        return false;
    }
}

// Client / Downstream
bool remote_process_mechanisms(pn_transport_t *transport, const char *mechs)
{
    pni_sasl_relay_t* impl = (pni_sasl_relay_t*) transport->sasl->impl_context;
    if (impl) {
        impl->mechlist = pn_strdup(mechs);
        if (notify_upstream(impl, DOWNSTREAM_MECHANISMS_RECEIVED)) {
            return true;
        } else {
            pni_sasl_set_desired_state(transport, SASL_ERROR);
            return false;
        }
    } else {
        return false;
    }
}

// Client / Downstream
void remote_process_challenge(pn_transport_t *transport, const pn_bytes_t *recv)
{
    pni_sasl_relay_t* impl = (pni_sasl_relay_t*) transport->sasl->impl_context;
    if (impl) {
        pni_copy_bytes(recv, &(impl->challenge));
        if (!notify_upstream(impl, DOWNSTREAM_CHALLENGE_RECEIVED)) {
            pni_sasl_set_desired_state(transport, SASL_ERROR);
        }
    }
}

// Client / Downstream
bool remote_process_outcome(pn_transport_t *transport)
{
    pni_sasl_relay_t* impl = (pni_sasl_relay_t*) transport->sasl->impl_context;
    if (impl) {
        impl->outcome = transport->sasl->outcome;
        impl->complete = true;
        if (notify_upstream(impl, DOWNSTREAM_OUTCOME_RECEIVED)) {
            return true;
        } else {
            pni_sasl_set_desired_state(transport, SASL_ERROR);
            return false;
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
        if (!notify_downstream(impl, UPSTREAM_INIT_RECEIVED)) {
            pni_sasl_set_desired_state(transport, SASL_ERROR);
        }
    }
}

// Server / Upstream
void remote_process_response(pn_transport_t *transport, const pn_bytes_t *recv)
{
    pni_sasl_relay_t* impl = (pni_sasl_relay_t*) transport->sasl->impl_context;
    if (impl) {
        pni_copy_bytes(recv, &(impl->response));
        if (!notify_downstream(impl, UPSTREAM_RESPONSE_RECEIVED)) {
            pni_sasl_set_desired_state(transport, SASL_ERROR);
        }
    }
}

void set_remote_impl(pn_transport_t *transport, pni_sasl_relay_t* context)
{
    pni_sasl_implementation remote_impl;
    remote_impl.free_impl = &remote_free;
    remote_impl.get_mechs = &remote_get_mechs;
    remote_impl.init_server = &remote_init_server;
    remote_impl.process_init = &remote_process_init;
    remote_impl.process_response = &remote_process_response;
    remote_impl.init_client = &remote_init_client;
    remote_impl.process_mechanisms = &remote_process_mechanisms;
    remote_impl.process_challenge = &remote_process_challenge;
    remote_impl.process_outcome = &remote_process_outcome;
    remote_impl.prepare = &remote_prepare;
    pni_sasl_set_implementation(transport, remote_impl, context);
}

void pn_use_remote_authentication_service(pn_transport_t *transport, const char* address)
{
    pni_sasl_relay_t* context = new_pni_sasl_relay_t(address);
    set_remote_impl(transport, context);
}

void pn_handle_authentication_service_connection_event(pn_event_t *e)
{
    pn_connection_t *conn = pn_event_connection(e);
    pn_transport_t *transport = pn_event_transport(e);
    if (pn_event_type(e) == PN_CONNECTION_BOUND) {
        pn_transport_logf(transport, "Handling connection bound event for authentication service connection");
        pni_sasl_relay_t* context = get_sasl_relay_context(conn);
        set_remote_impl(pn_event_transport(e), context);
    } else if (pn_event_type(e) == PN_CONNECTION_REMOTE_OPEN) {
        pn_transport_logf(transport, "authentication against service complete; closing connection");
        pn_connection_close(conn);
    } else if (pn_event_type(e) == PN_CONNECTION_REMOTE_CLOSE) {
        pn_transport_logf(transport, "authentication service closed connection");
        pn_connection_close(conn);
        pn_transport_close_head(transport);
    } else if (pn_event_type(e) == PN_TRANSPORT_CLOSED) {
        pn_transport_logf(transport, "disconnected from authentication service");
        pni_sasl_relay_t* impl = (pni_sasl_relay_t*) transport->sasl->impl_context;
        if (!impl->complete) {
            notify_upstream(impl, DOWNSTREAM_CLOSED);
        }
    } else {
        pn_transport_logf(transport, "Ignoring event for authentication service connection: %s", pn_event_type_name(pn_event_type(e)));
    }
}
