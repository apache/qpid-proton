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
 * This SSL implementation defines the following objects:

 * @li A top-level object that stores the configuration used by one or more SSL
 * sessions (pn_ssl_domain_t).
 * @li A per-connection SSL session object that performs the encryption/authentication
 * associated with the transport (pn_ssl_t).
 * @li The encryption parameters negotiated for the SSL session (pn_ssl_state_t).
 *
 * A pn_ssl_domain_t object must be created and configured before an SSL session can be
 * established.  The pn_ssl_domain_t is used to construct an SSL session (pn_ssl_t).  The
 * session "adopts" its configuration from the pn_ssl_domain_t that was used to create it.
 * For example, pn_ssl_domain_t can be configured as either a "client" or a "server".  SSL
 * sessions constructed from this domain will perform the corresponding role (either
 * client or server).
 *
 * Some per-session attributes - such as peer verification mode - may be overridden on a
 * per-session basis from the default provided by the parent pn_ssl_domain_t.
 *
 * If either an SSL server or client needs to identify itself with the remote node, it
 * must have its SSL certificate configured (see ::pn_ssl_domain_set_credentials()).
 *
 * If either an SSL server or client needs to verify the identity of the remote node, it
 * must have its database of trusted CAs configured (see ::pn_ssl_domain_set_trusted_ca_db()).
 *
 * An SSL server connection may allow the remote client to connect without SSL (eg. "in
 * the clear"), see ::pn_ssl_allow_unsecured_client().
 *
 * The level of verification required of the remote may be configured (see
 * ::pn_ssl_domain_set_default_peer_authentication ::pn_ssl_set_peer_authentication,
 * ::pn_ssl_get_peer_authentication).
 *
 * Support for SSL Client Session resume is provided (see ::pn_ssl_get_state,
 * ::pn_ssl_resume_state).
 */

typedef struct pn_ssl_domain_t pn_ssl_domain_t;
typedef struct pn_ssl_t pn_ssl_t;
typedef struct pn_ssl_state_t pn_ssl_state_t;

/** Determines the type of SSL endpoint. */
typedef enum {
  PN_SSL_MODE_CLIENT=1, /**< Local connection endpoint is an SSL client */
  PN_SSL_MODE_SERVER    /**< Local connection endpoint is an SSL server */
} pn_ssl_mode_t;

/** Create an SSL configuration domain
 *
 * This method allocates an SSL domain object.  This object is used to hold the SSL
 * configuration for one or more SSL sessions.  The SSL session object (pn_ssl_t) is
 * allocated from this object.
 *
 * @param[in] mode the role, client or server, assumed by all SSL sessions created
 * with this domain.
 * @return a pointer to the SSL domain, if SSL support is present.
 */
pn_ssl_domain_t *pn_ssl_domain( pn_ssl_mode_t mode);

/** Release an SSL configuration domain
 *
 * This method frees an SSL domain object allocated by ::pn_ssl_domain.
 * @param[in] domain the domain to destroy.
 */
void pn_ssl_domain_free( pn_ssl_domain_t *domain );

/** Set the certificate that identifies the local node to the remote.
 *
 * This certificate establishes the identity for the local node for all SSL sessions
 * created from this domain.  It will be sent to the remote if the remote needs to verify
 * the identity of this node.  This may be used for both SSL servers and SSL clients (if
 * client authentication is required by the server).
 *
 * @note This setting effects only those pn_ssl_t objects created after this call
 * returns.  pn_ssl_t objects created before invoking this method will use the domain's
 * previous setting.
 *
 * @param[in] domain the ssl domain that will use this certificate.
 * @param[in] certificate_file path to file/database containing the identifying
 * certificate.
 * @param[in] private_key_file path to file/database containing the private key used to
 * sign the certificate
 * @param[in] password the password used to sign the key, else NULL if key is not
 * protected.
 * @return 0 on success
 */
int pn_ssl_domain_set_credentials( pn_ssl_domain_t *domain,
                               const char *certificate_file,
                               const char *private_key_file,
                               const char *password);

/** Configure the set of trusted CA certificates used by this domain to verify peers.
 *
 * If the local SSL client/server needs to verify the identity of the remote, it must
 * validate the signature of the remote's certificate.  This function sets the database of
 * trusted CAs that will be used to verify the signature of the remote's certificate.
 *
 * @note This setting effects only those pn_ssl_t objects created after this call
 * returns.  pn_ssl_t objects created before invoking this method will use the domain's
 * previous setting.
 *
 * @param[in] domain the ssl domain that will use the database.
 * @param[in] certificate_db database of trusted CAs, used to authenticate the peer.
 * @return 0 on success
 */
int pn_ssl_domain_set_trusted_ca_db(pn_ssl_domain_t *domain,
                                const char *certificate_db);

/** Determines the level of peer validation.
 *
 *  VERIFY_PEER will only connect to those peers that provide a valid identifying
 *  certificate signed by a trusted CA and are using an authenticated cipher.
 *  ANONYMOUS_PEER does not require a valid certificate, and permits use of ciphers that
 *  do not provide authentication.
 *
 *  ANONYMOUS_PEER is configured by default.
 */
typedef enum {
  PN_SSL_VERIFY_NULL=0,   /**< internal use only */
  PN_SSL_VERIFY_PEER,     /**< require peer to provide a valid identifying certificate */
  PN_SSL_ANONYMOUS_PEER,  /**< do not require a certificate nor cipher authorization */
} pn_ssl_verify_mode_t;

/** Configure the level of verification used on the peer certificate.
 *
 * This method controls how the peer's certificate is validated, if at all.  By default,
 * neither servers nor clients attempt to verify their peers (PN_SSL_ANONYMOUS_PEER).
 * Once certificates and trusted CAs are configured, peer verification can be enabled.
 *
 * @note In order to verify a peer, a trusted CA must be configured. See
 * ::pn_ssl_set_trusted_ca_db().
 *
 * @note Servers must provide their own certificate when verifying a peer.  See
 * ::pn_ssl_set_credentials().
 *
 * @note This method sets the default behavior for all SSL sessions associated with
 * this domain. This attribute can be overridden on a per-session basis if desired (see
 * ::pn_ssl_set_default_peer_authentication)
 *
 * @note This setting effects only those pn_ssl_t objects created after this call
 * returns.  pn_ssl_t objects created before invoking this method will use the domain's
 * previous setting.
 *
 * @param[in] domain the ssl domain to configure.
 * @param[in] mode the level of validation to apply to the peer
 * @param[in] trusted_CAs path to a database of trusted CAs that the server will advertise
 * to the peer client if the server has been configured to verify its peer.
 * @return 0 on success
 */
int pn_ssl_domain_set_default_peer_authentication(pn_ssl_domain_t *domain,
                                                  const pn_ssl_verify_mode_t mode,
                                                  const char *trusted_CAs);

/** Create an SSL session for a given transport.
 *
 * A transport must have an SSL object in order to "speak" SSL over its connection. This
 * method allocates an SSL object using the given domain, and associates it with the
 * transport.
 *
 * @param[in] domain the ssl domain used to configure the SSL session.
 * @param[in] transport the transport that will use the SSL session.
 * @return a pointer to the SSL object configured for this transport.  Returns NULL if SSL
 * cannot be provided, which would occur if no SSL support is available.
 */
pn_ssl_t *pn_ssl_new( pn_ssl_domain_t *domain, pn_transport_t *transport);

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

/** Override the default verification level for this session.
 *
 * This method can be used to override the default verification level provided by the
 * parent domain. See ::pn_ssl_domain_set_default_peer_authentication. It must be called
 * before the session is used to transfer data.
 *
 * @param[in] ssl the ssl client/server to configure.
 * @param[in] mode the level of validation to apply to the peer
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
 * @param[in] ssl the ssl client/server to query.
 * @param[out] mode the level of validation that will be applied to the peer's certificate.
 * @param[out] trusted_CAs set to a buffer to hold the path to the database of trusted CAs
 * that the server will advertise to the peer client. If NULL, the path will not be
 * returned.
 * @param[in,out] trusted_CAs_size on input set to the number of octets in trusted_CAs.
 * on output, set to the number of octets needed to hold the value of trusted_CAs plus a
 * null byte.
 * @return 0 on success
 */
int pn_ssl_get_peer_authentication(pn_ssl_t *ssl,
                                   pn_ssl_verify_mode_t *mode,
                                   char *trusted_CAs, size_t *trusted_CAs_size);

/** Get the name of the Cipher that is currently in use.
 *
 * Gets a text description of the cipher that is currently active, or returns FALSE if SSL
 * is not active (no cipher).  Note that the cipher in use may change over time due to
 * renegotiation or other changes to the SSL state.
 *
 * @param[in] ssl the ssl client/server to query.
 * @param[in,out] buffer buffer of size bytes to hold cipher name
 * @param[in] size maximum number of bytes in buffer.
 * @return True if cipher name written to buffer, False if no cipher in use.
 */
bool pn_ssl_get_cipher_name(pn_ssl_t *ssl, char *buffer, size_t size);

/** Get the name of the SSL protocol that is currently in use.
 *
 * Gets a text description of the SSL protocol that is currently active, or returns FALSE if SSL
 * is not active.  Note that the protocol may change over time due to renegotiation.
 *
 * @param[in] ssl the ssl client/server to query.
 * @param[in,out] buffer buffer of size bytes to hold the version identifier
 * @param[in] size maximum number of bytes in buffer.
 * @return True if the version information was written to buffer, False if SSL connection
 * not ready.
 */
bool pn_ssl_get_protocol_name(pn_ssl_t *ssl, char *buffer, size_t size);

/** Obtain a handle to the SSL parameters negotiated for this SSL session.
 *
 * Used for client session resume.  Returns a handle to the negotiated SSL parameters
 * that may be restored on a later connection attempt to the same peer.  See
 * ::pn_ssl_resume_state.  Must be released by calling :pn_ssl_state_free when done.
 *
 * @note The state is only valid once an SSL session has been sucessfully established. It
 * is therefore recommended to retrieve the state after the transport has successfully
 * exchanged data with the peer, prior to shutting down the connection.
 *
 * @param[in] ssl the session whose negotiated parameters are to be restored at a later date.
 * @return a pointer to the negotated state, or NULL if session resume is not supported.
 */
pn_ssl_state_t *pn_ssl_get_state( pn_ssl_t *ssl );

/** Resume previously negotiated parameters on a new session.
 *
 * Used for client session resume.  State can only be resumed on new connections to the
 * same peer as the original session and using the same configuration parameters. See
 * ::pn_ssl_get_state.
 *
 * @note This method must be called prior to allowing traffic over the connection.
 *
 * @note This is a best-effort service - there is no guarantee that the remote server will
 * accept the resumed parameters.  The remote server may choose to ignore these
 * parameters, and request a re-negotiation instead.
 *
 * @param[in] ssl the session for with the parameters should be used.
 * @param[in] state the previously negotiated parameters to use.
 * @return 0 if the ssl session will accept the state parameter.  This does not guarantee
 * that the remote will allow the state to be resumed (see ::pn_ssl_state_resumed_ok).
 */
int pn_ssl_resume_state( pn_ssl_t *ssl, pn_ssl_state_t *state );

/** Check whether the state has been resumed.
 *
 * Used for client session resume.  When called on an active session, indicates whether
 * the state has been resumed from a previous session.
 *
 * @note This is a best-effort service - there is no guarantee that the remote server will
 * accept the resumed parameters.  The remote server may choose to ignore these
 * parameters, and request a re-negotiation instead.
 *
 * @param[in] ssl the ssl session to check
 * @return true if the ssl session was resumed, false if the Server required a
 * re-negotiation instead.
 */
bool pn_ssl_state_resumed_ok( pn_ssl_t *ssl );

/** Release a state handle previously obtained via ::pn_ssl_get_state.
 *
 * Used for client session resume.  Once obtained, the state remains valid until it is
 * freed using this method.  This allows the state to remain valid after the original SSL
 * session-related objects (pn_ssl_t and/or pn_ssl_domain_t) have been closed/deallocated.
 *
 * @param[in] state the state object that will be released.
 */
void pn_ssl_state_free( pn_ssl_state_t *state );




  /** original API: */

/** Get the SSL session object associated with a transport.
 *
 * This method returns the SSL object associated with the transport.
 *
 * @return a pointer to the SSL object configured for this transport.  Returns NULL if
 * no SSL session is associated with the transport.
 *
 * @deprecated The semantics have changed - need to deprecate old behavior.
 */
pn_ssl_t *pn_ssl(pn_transport_t *transport);

/** @deprecated see ::pn_ssl_domain, ::pn_ssl_new */
int pn_ssl_init(pn_ssl_t *ssl, pn_ssl_mode_t mode);
/** @deprecated see ::pn_ssl_domain_set_credentails */
int pn_ssl_set_credentials( pn_ssl_t *ssl,
                            const char *certificate_file,
                            const char *private_key_file,
                            const char *password);
/** @deprecated see ::pn_ssl_domain_set_trusted_ca_db */
int pn_ssl_set_trusted_ca_db(pn_ssl_t *ssl,
                             const char *certificate_db);

  
#ifdef __cplusplus
}
#endif

#endif /* ssl.h */
