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
#include "ssl-internal.h"


/** @file
 * SSL/TLS support API.
 *
 * This file contains stub implementations of the SSL/TLS API.  This implementation is
 * used if there is no SSL/TLS support in the system's environment.
 */

pn_ssl_t *pn_ssl(pn_transport_t *transport)
{
  return NULL;
}

int pn_ssl_init(pn_ssl_t *ssl, pn_ssl_mode_t mode)
{
  return -1;
}


int pn_ssl_set_credentials(pn_ssl_t *ssl,
                           const char *certificate_file,
                           const char *private_key_file,
                           const char *password)
{
  return -1;
}

int pn_ssl_set_trusted_ca_db(pn_ssl_t *ssl,
                             const char *certificate_db)
{
  return -1;
}

int pn_ssl_allow_unsecured_client(pn_ssl_t *ssl)
{
  return -1;
}


int pn_ssl_set_peer_authentication(pn_ssl_t *ssl,
                                   const pn_ssl_verify_mode_t mode,
                                   const char *trusted_CAs)
{
  return -1;
}


int pn_ssl_get_peer_authentication(pn_ssl_t *ssl,
                                   pn_ssl_verify_mode_t *mode,
                                   char *trusted_CAs, size_t *trusted_CAs_size)
{
  return -1;
}

void pn_ssl_free( pn_ssl_t *ssl)
{
}

void pn_ssl_trace(pn_ssl_t *ssl, pn_trace_t trace)
{
}

ssize_t pn_ssl_input(pn_ssl_t *ssl, const char *bytes, size_t available)
{
  return PN_EOS;
}

ssize_t pn_ssl_output(pn_ssl_t *ssl, char *buffer, size_t max_size)
{
  return PN_EOS;
}

bool pn_ssl_get_cipher_name(pn_ssl_t *ssl, char *buffer, size_t size)
{
  return false;
}

bool pn_ssl_get_protocol_name(pn_ssl_t *ssl, char *buffer, size_t size)
{
  return false;
}

int pn_ssl_set_peer_hostname( pn_ssl_t *ssl, const char *hostname)
{
  return -1;
}

int pn_ssl_get_peer_hostname( pn_ssl_t *ssl, char *hostname, size_t *bufsize )
{
  return -1;
}
