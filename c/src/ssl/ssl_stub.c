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

#include <proton/ssl.h>
#include <proton/error.h>
#include <proton/transport.h>
#include "core/engine-internal.h"


/** @file
 * SSL/TLS support API.
 *
 * This file contains stub implementations of the SSL/TLS API.  This implementation is
 * used if there is no SSL/TLS support in the system's environment.
 */

bool pn_ssl_present(void)
{
  return false;
}

pn_ssl_t *pn_ssl(pn_transport_t *transport)
{
  return NULL;
}

int pn_ssl_init(pn_ssl_t *ssl, pn_ssl_domain_t *domain,
                const char *session_id)
{
  return -1;
}

void pn_ssl_free(pn_transport_t *transport)
{
}

void pn_ssl_trace(pn_ssl_t *ssl, pn_trace_t trace)
{
}

ssize_t pn_ssl_input(pn_transport_t *transport, unsigned int layer, const char *bytes, size_t available)
{
  return PN_EOS;
}

ssize_t pn_ssl_output(pn_transport_t *transport, unsigned int layer, char *buffer, size_t max_size)
{
  return PN_EOS;
}

const pn_io_layer_t ssl_layer = {
    pn_ssl_input,
    pn_ssl_output,
    NULL,
    NULL
};

bool pn_ssl_get_cipher_name(pn_ssl_t *ssl, char *buffer, size_t size)
{
  *buffer = '\0';
  return false;
}

bool pn_ssl_get_protocol_name(pn_ssl_t *ssl, char *buffer, size_t size)
{
  *buffer = '\0';
  return false;
}

pn_ssl_domain_t *pn_ssl_domain( pn_ssl_mode_t mode)
{
  return NULL;
}

void pn_ssl_domain_free( pn_ssl_domain_t *d )
{
}

int pn_ssl_domain_set_credentials( pn_ssl_domain_t *domain,
                               const char *certificate_file,
                               const char *private_key_file,
                               const char *password)
{
  return -1;
}

int pn_ssl_domain_set_trusted_ca_db(pn_ssl_domain_t *domain,
                                const char *certificate_db)
{
  return -1;
}

int pn_ssl_domain_set_peer_authentication(pn_ssl_domain_t *domain,
                                   const pn_ssl_verify_mode_t mode,
                                   const char *trusted_CAs)
{
  return -1;
}

int pn_ssl_domain_allow_unsecured_client(pn_ssl_domain_t *domain)
{
  return -1;
}

int pn_ssl_domain_set_ciphers(pn_ssl_domain_t *domain, const char *ciphers)
{
  return -1;
}

int pn_ssl_domain_set_protocols(pn_ssl_domain_t* domain, const char* protocols)
{
  return -1;
}

bool pn_ssl_allow_unsecured(pn_ssl_t *ssl)
{
  return true;
}

pn_ssl_resume_status_t pn_ssl_resume_status( pn_ssl_t *s )
{
  return PN_SSL_RESUME_UNKNOWN;
}

int pn_ssl_set_peer_hostname( pn_ssl_t *ssl, const char *hostname)
{
  return -1;
}

int pn_ssl_get_peer_hostname( pn_ssl_t *ssl, char *hostname, size_t *bufsize )
{
  return -1;
}

const char* pn_ssl_get_remote_subject(pn_ssl_t *ssl)
{
  return NULL;
}

int pn_ssl_get_ssf(pn_ssl_t *ssl)
{
  return 0;
}

int pn_ssl_get_cert_fingerprint(pn_ssl_t *ssl0, char *fingerprint, size_t fingerprint_length, pn_ssl_hash_alg hash_alg)
{
    *fingerprint = '\0';
    return -1;
}

const char* pn_ssl_get_remote_subject_subfield(pn_ssl_t *ssl0, pn_ssl_cert_subject_subfield field)
{
    return NULL;
}
