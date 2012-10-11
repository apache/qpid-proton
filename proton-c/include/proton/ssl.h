#ifndef PROTON_SSL_H
#define PROTON_SSL_H 1

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

#include <sys/types.h>
#include <stdbool.h>
#include <proton/engine.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @file
 * API for using SSL with the Transport Layer.
 *
 * A Transport may be configured to use SSL for encryption and/or authentication.  A
 * Transport can be configured as either an "SSL client" or an "SSL server".  An SSL
 * client is the party that proactively establishes a connection to an SSL server.  An SSL
 * server is the party that accepts a connection request from a remote SSL client.
 *
 * If either an SSL server or client needs to identify itself with the remote node, it
 * must have its SSL certificate configured (see ::pn_ssl_set_credentials()).
 *
 * If either an SSL server or client needs to verify the identity of the remote node, it
 * must have its database of trusted CAs configured (see ::pn_ssl_set_trusted_ca_db()).
 *
 * An SSL server may allow peers to connect without SSL (eg. "in the clear"), see
 * ::pn_ssl_allow_unsecured_client().
 *
 * The level of verification required of the remote may be configured (see
 * ::pn_ssl_set_peer_authentication, ::pn_ssl_get_peer_authentication).
 */

typedef struct pn_ssl_t pn_ssl_t;

/** Get the SSL  object associated with a transport.
 *
 * This method returns the SSL object associated with the transport.  If no SSL object
 * exists, one will be allocated and returned.  A transport must have a configured SSL
 * object in order to "speak" SSL over its connection.
 *
 * By default, a new SSL object is configured to be a Client.  Use :pn_ssl_init to change
 * the SSL object's mode to Server if desired.
 *
 * @return a pointer to the SSL object configured for this transport.  Returns NULL if SSL
 * cannot be provided, which would occur if no SSL support is available.
 */
pn_ssl_t *pn_ssl(pn_transport_t *transport);

/** Initialize the pn_ssl_t object.
 *
 * An SSL object be either an SSL server or an SSL client.  It cannot be both. Those
 * transports that will be used to accept incoming connection requests must be configured
 * as an SSL server. Those transports that will be used to initiate outbound connections
 * must be configured as an SSL client.
 *
 * @return 0 if configuration succeeded, else an error code.
 */
typedef enum {
  PN_SSL_MODE_CLIENT=1, /**< Local connection endpoint is an SSL client */
  PN_SSL_MODE_SERVER    /**< Local connection endpoint is an SSL server */
} pn_ssl_mode_t;
int pn_ssl_init(pn_ssl_t *ssl, pn_ssl_mode_t mode);

/** Set the certificate that identifies the local node to the remote.
 *
 * This certificate establishes the identity for the local node.  It will be sent to the
 * remote if the remote needs to verify the identity of this node.  This may be used for
 * both SSL servers and SSL clients (if client authentication is required by the server).
 *
 * @param[in] ssl the ssl server/client will provide this certificate.
 * @param[in] certificate_path path to file/database containing the identifying
 * certificate.
 * @param[in] private_key_path path to file/database containing the private key used to
 * sign the certificate
 * @param[in] password the password used to sign the key, else NULL if key is not
 * protected.
 * @return 0 on success
 */
 int pn_ssl_set_credentials( pn_ssl_t *ssl,
                             const char *certificate_file,
                             const char *private_key_file,
                             const char *password);

/** Configure the set of trusted CA certificates used by this node to verify peers.
 *
 * If the local SSL client/server needs to verify the identity of the remote, it must
 * validate the signature of the remote's certificate.  This function sets the database of
 * trusted CAs that will be used to verify the signature of the remote's certificate.
 *
 * @param[in] ssl the ssl server/client that will use the database.
 * @param[in] certificate_db database of trusted CAs, used to authenticate the peer.
 * @return 0 on success
 */

int pn_ssl_set_trusted_ca_db(pn_ssl_t *ssl,
                             const char *certificate_db);

/** Permit a server to accept connection requests from non-SSL clients.
 *
 * This configures the server to "sniff" the incoming client data stream, and dynamically
 * determine whether SSL/TLS is being used.  This option is disabled by default: only
 * clients using SSL/TLS are accepted.
 *
 * @param[in] ssl the SSL server that will accept the client connection.
 * @return 0 on success
 */
int pn_ssl_allow_unsecured_client(pn_ssl_t *ssl);


/** Determines the level of peer validation.
 *
 *  VERIFY_PEER will only connect to those peers that provide a valid identifying
 *  certificate signed by a trusted CA and are using an authenticated cipher.
 *  NO_VERIFY_PEER does not require the peer to provide a certificate, but does require
 *  use of an authenticated ciper.  ANONYMOUS_PEER not only does not require a valid
 *  certificate, but it permits use of ciphers that do not provide authentication.
 *
 *  By default, a client will use VERIFY_PEER.
 *
 *  A server will initially use ANONYMOUS_PEER until a certificate is configured via
 *  ::pn_ssl_set_credentials(), then the server will use NO_VERIFY_PEER.
 *
 *  These default settings can be changed via ::pn_ssl_set_peer_authentication()
 */
typedef enum {
  PN_SSL_VERIFY_PEER,     /**< require peer to provide a valid identifying certificate */
  PN_SSL_NO_VERIFY_PEER,  /**< do not require peer to provide an identifying certificate */
  PN_SSL_ANONYMOUS_PEER,  /**< do not require a certificate nor cipher authorization */
} pn_ssl_verify_mode_t;


/** Configure the level of verification used on the peer certificate.
 *
 * This method controls how the peer's certificate is validated, if at all.  By default,
 * SSL servers do not attempt to verify their peers (PN_SSL_NO_VERIFY), and SSL clients
 * require the remote to provide a valid certificate (PN_SSL_VERIFY_PEER).
 *
 * @note In order to verify a peer, a trusted CA must be configured. See
 * ::pn_ssl_set_trusted_ca_db().
 *
 * @note Servers must provide their own certificate when verifying a peer.  See
 * ::pn_ssl_set_credentials().
 *
 * @param[in] ssl the ssl client/server to configure.
 * @param[in] mode the level of validation to apply to the peer's certificate.
 * @param[in] trusted_CAs path to a database of trusted CAs that the server will advertise
 * to the peer client if the server has been configured to verify its peer.
 * @return 0 on success
 */
int pn_ssl_set_peer_authentication(pn_ssl_t *ssl,
                                   const pn_ssl_verify_mode_t mode,
                                   const char *trusted_CAs);

/** Get the level of verification to be used on the peer certificate.
 *
 * Access the current peer certificate validation level.  See
 * ::pn_ssl_set_peer_authentication().
 *
 *
 * @param[in] ssl the ssl client/server to query.
 * @param[out] mode the level of validation that will be applied to the peer's certificate.
 * @param[out] trusted_CAs set to a buffer to hold the path to the database of trusted CAs
 * that the server will advertise to the peer client. If NULL, the path will not be
 * returned.
 * @param[in,out] trusted_CAs_size on input set to the number of octets in trusted_CAs.
 * on output, set to the number of octets needed to hold the value of trusted_CAs plus a
 * null byte.  @return 0 on success
 */
int pn_ssl_get_peer_authentication(pn_ssl_t *ssl,
                                   pn_ssl_verify_mode_t *mode,
                                   char *trusted_CAs, size_t *trusted_CAs_size);


#ifdef __cplusplus
}
#endif

#endif /* ssl.h */
