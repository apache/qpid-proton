#ifndef PROTON_TLS_H
#define PROTON_TLS_H 1

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

#include <proton/import_export.h>
#include <proton/raw_connection.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 *
 * @copybrief tls
 *
 * @addtogroup tls
 * @{
 */

/**
 * API for using TLS separate from AMQP connections.
 *
 * This API is currently unsettled and subject to significant change and improvement.
 *
 * Based heavily on the original Proton SSL API for configuring TLS over AMQP connections,
 * this implementation separates the encryption/decryption of data from the network IO
 * operations.
 *
 * A Transport can be configured as either an "TLS client" or an "TLS server".  An TLS
 * client is the party that proactively establishes a connection to an TLS server.  An TLS
 * server is the party that accepts a connection request from a remote TLS client.
 *
 * This TLS implementation defines the following objects:

 * @li A top-level object that stores the configuration used by one or more TLS
 * sessions (pn_tls_config_t).
 * @li A per-connection TLS session object that performs the encryption/authentication
 * associated with the transport (pn_tls_t).
 *
 * The pn_tls_config_t is used to construct an TLS session (pn_tls_t).  The
 * session "adopts" its configuration from the pn_tls_config_t that was used to create it.
 * For example, pn_tls_config_t can be configured as either a "client" or a "server".  TLS
 * sessions constructed from this domain will perform the corresponding role (either
 * client or server).
 *
 * If an TLS session is created without a pn_tls_config_t object then a default will be used
 * (see ::pn_tls_init()).
 *
 * If either an TLS server or client needs to identify itself with the remote node, it
 * must have its TLS certificate configured (see ::pn_tls_config_set_credentials()).
 *
 * If either an TLS server or client needs to verify the identity of the remote node, it
 * must have its database of trusted CAs configured. By default this will be set up to use
 * the default system database of trusted CA. But this can be changed
 * (see ::pn_tls_config_set_trusted_certs()).
 *
 * The level of verification required of the remote may be configured (see
 * ::pn_tls_config_set_peer_authentication)
 */
typedef struct pn_tls_config_t pn_tls_config_t;

/**
 * @see pn_tls
 */
typedef struct pn_tls_t pn_tls_t;

/**
 * Determines the type of TLS endpoint.
 */
typedef enum {
  PN_TLS_MODE_CLIENT = 1, /**< Local connection endpoint is an TLS client */
  PN_TLS_MODE_SERVER      /**< Local connection endpoint is an TLS server */
} pn_tls_mode_t;

/**
 * Error codes
 */

#define PN_TLS_OK (0)                   /**< No error */
#define PN_TLS_INIT_ERR (-1)            /**< Failure in initialization, unrelated to activity with the peer */
#define PN_TLS_PROTOCOL_ERR (-2)        /**< Failure in the TLS protocol between peers */
#define PN_TLS_AUTHENTICATION_ERR (-3)  /**< Peer authentication failure */
#define PN_TLS_STATE_ERR (-4)           /**< Requested action not possible due to session state */


/**
 * Create an TLS configuration domain
 *
 * This method allocates an TLS domain object.  This object is used to hold the TLS
 * configuration for one or more TLS sessions.  The TLS session object (pn_tls_t) is
 * allocated from this object.
 *
 * @param[in] mode the role, client or server, assumed by all TLS sessions created
 * with this domain.
 * @return a pointer to the TLS domain, if TLS support is present.
 */
PN_TLS_EXTERN pn_tls_config_t *pn_tls_config(pn_tls_mode_t mode);

/**
 * Release an TLS configuration domain
 *
 * This method frees an TLS domain object allocated by ::pn_tls_config.
 * @param[in] domain the domain to destroy.
 */
PN_TLS_EXTERN void pn_tls_config_free(pn_tls_config_t *domain);

/**
 * Set the certificate that identifies the local node to the remote.
 *
 * This certificate establishes the identity for the local node for all TLS sessions
 * created from this domain.  It will be sent to the remote if the remote needs to verify
 * the identity of this node.  This may be used for both TLS servers and TLS clients (if
 * client authentication is required by the server).
 *
 * @note This setting effects only those pn_tls_t objects created after this call
 * returns.  pn_tls_t objects created before invoking this method will use the domain's
 * previous setting.
 *
 * @param[in] domain the tls domain that will use this certificate.
 * @param[in] credential_1 specifier for the file/database containing the identifying
 * certificate. For Openssl users, this is a PEM file. For Windows SChannel users, this is
 * the PKCS#12 file or system store.
 * @param[in] credential_2 an optional key to access the identifying certificate. For
 * Openssl users, this is an optional PEM file containing the private key used to sign the
 * certificate. For Windows SChannel users, this is the friendly name of the
 * self-identifying certificate if there are multiple certificates in the store.
 * @param[in] password the password used to sign the key, else NULL if key is not
 * protected.
 * @return 0 on success
 */
PN_TLS_EXTERN int  pn_tls_config_set_credentials(pn_tls_config_t *domain,
                                            const char *credential_1,
                                            const char *credential_2,
                                            const char *password);

/**
 * Configure the set of trusted CA certificates used by this domain to verify peers.
 *
 * If the local TLS client/server needs to verify the identity of the remote, it must
 * validate the signature of the remote's certificate.  This function sets the database of
 * trusted CAs that will be used to verify the signature of the remote's certificate.
 *
 * @note This setting effects only those pn_tls_t objects created after this call
 * returns.  pn_tls_t objects created before invoking this method will use the domain's
 * previous setting.
 *
 * @note By default the list of trusted CA certificates will be set to the system default.
 * What this is is depends on the OS and the TLS implementation used: For OpenTLS the default
 * will depend on how the OS is set up. When using the Windows SChannel implementation the default
 * will be the users default trusted certificate store.
 *
 * @param[in] domain the tls domain that will use the database.
 * @param[in] certificate_db database of trusted CAs, used to authenticate the peer.
 * @return 0 on success
 */
PN_TLS_EXTERN int pn_tls_config_set_trusted_certs(pn_tls_config_t *domain,
                                const char *certificate_db);

/**
 * Determines the level of peer validation.
 *
 * ANONYMOUS_PEER does not require a valid certificate, and permits
 * use of ciphers that do not provide authentication.
 *
 * VERIFY_PEER will only connect to those peers that provide a valid
 * identifying certificate signed by a trusted CA and are using an
 * authenticated cipher.
 *
 * VERIFY_PEER_NAME is like VERIFY_PEER, but also requires the peer's
 * identity as contained in the certificate to be valid (see
 * ::pn_tls_set_peer_hostname).
 *
 * VERIFY_PEER_NAME is configured by default.
 */
typedef enum {
  PN_TLS_VERIFY_NULL = 0,   /**< internal use only */
  PN_TLS_VERIFY_PEER,       /**< require peer to provide a valid identifying certificate */
  PN_TLS_ANONYMOUS_PEER,    /**< do not require a certificate nor cipher authorization */
  PN_TLS_VERIFY_PEER_NAME   /**< require valid certificate and matching name */
} pn_tls_verify_mode_t;

/**
 * Configure the level of verification used on the peer certificate.
 *
 * This method controls how the peer's certificate is validated, if at all.  By default,
 * servers do not attempt to verify their peers (PN_TLS_ANONYMOUS_PEER) but
 * clients attempt to verify both the certificate and peer name (PN_TLS_VERIFY_PEER_NAME).
 * Once certificates and trusted CAs are configured, peer verification can be enabled.
 *
 * @note In order to verify a peer, a trusted CA must be configured. See
 * ::pn_tls_config_set_trusted_certs().
 *
 * @note Servers must provide their own certificate when verifying a peer.  See
 * ::pn_tls_config_set_credentials().
 *
 * @note This setting effects only those pn_tls_t objects created after this call
 * returns.  pn_tls_t objects created before invoking this method will use the domain's
 * previous setting.
 *
 * @param[in] domain the tls domain to configure.
 * @param[in] mode the level of validation to apply to the peer
 * @param[in] trusted_CAs path to a database of trusted CAs that the server will advertise
 * to the peer client if the server has been configured to verify its peer.
 * @return 0 on success
 */
PN_TLS_EXTERN int pn_tls_config_set_peer_authentication(pn_tls_config_t *domain,
                                                    const pn_tls_verify_mode_t mode,
                                                    const char *trusted_CAs);

/**
 * Configure the list of permitted ciphers
 *
 * @note The syntax of the permitted list is undefined and will depend on the
 * underlying TLS implementation.
 *
 * @param[in] domain the tls domain to configure.
 * @param[in] ciphers string representing the cipher list
 * @return 0 on success
 */
PN_TLS_EXTERN int pn_tls_config_set_impl_ciphers(pn_tls_config_t *domain, const char *ciphers);

/**
 * Create a new TLS session object derived from a domain.
 *
 * @param[in] domain the domain that configures the TLS session.
 * @return a pointer to the TLS object.  Returns NULL if memory allocation fails or if domain is NULL.
 */
PN_TLS_EXTERN pn_tls_t *pn_tls(pn_tls_config_t *domain);

/**
 * Start a TLS session.
 *
 * This method starts the operation of a TLS session based on the object's
 * configuration.  Subsequent configuration steps will have no effect.  A client
 * TLS session will generate one or more result buffers for the clienthello
 * handshake.
 * @param[in] tls the tls session to configured.
 * @return 0 on success, else an error code.
 */
PN_TLS_EXTERN int pn_tls_start(pn_tls_t *tls);


PN_TLS_EXTERN void pn_tls_free(pn_tls_t *tls);

/**
 * Get the name of the Cipher that is currently in use.
 *
 * Gets a text description of the cipher that is currently active, or
 * returns FALSE if TLS is not active (no cipher).  Note that the
 * cipher in use may change over time due to renegotiation or other
 * changes to the TLS state.  The cipher is not null terminated.
 *
 * @param[in] tls the tls client/server to query.
 * @param[out] cipher set to a pointer to the current cipher description or NULL if no cipher.
 * @param[out] size set to the cipher or 0 if no cipher.
 * @return True if cipher is non-null and size is not zero.
 */
PN_TLS_EXTERN bool pn_tls_get_cipher(pn_tls_t *tls, const char **cipher, size_t *size);

/**
 * Get the SSF (security strength factor) of the Cipher that is currently in use.
 *
 * @param[in] tls the tls client/server to query.
 * @return the ssf, note that 0 means no security.
 */
PN_TLS_EXTERN int pn_tls_get_ssf(pn_tls_t *tls);

/**
 * Get the name of the TLS protocol that is currently in use.
 *
 * Gets a text description of the TLS protocol that is currently active, or returns FALSE if TLS
 * is not active.  Note that the protocol may change over time due to renegotiation.
 *
 * @param[in] tls the tls client/server to query.
 * @param[out] version set to a pointer to the current protocol version or NULL if not active.
 * @param[out] size set to the length of the version or zero if not active.
 * @return True if version is non-null and size is not zero.
 * not ready.
 */
PN_TLS_EXTERN bool pn_tls_get_protocol_version(pn_tls_t *tls, const char **version, size_t *size);

/**
 * Set the expected identity of the remote peer.
 *
 * By default, TLS will use the hostname associated with the connection that
 * the transport is bound to (see ::pn_connection_set_hostname).  This method
 * allows the caller to override that default.
 *
 * The hostname is used for two purposes: 1) when set on an TLS client, it is sent to the
 * server during the handshake (if Server Name Indication is supported), and 2) it is used
 * to check against the identifying name provided in the peer's certificate. If the
 * supplied name does not exactly match a SubjectAltName (type DNS name), or the
 * CommonName entry in the peer's certificate, the peer is considered unauthenticated
 * (potential imposter), and the TLS connection is aborted.
 *
 * @note Verification of the hostname is only done if PN_TLS_VERIFY_PEER_NAME is enabled.
 * See ::pn_tls_config_set_peer_authentication.
 *
 * @param[in] tls the tls session.
 * @param[in] hostname the expected identity of the remote. Must conform to the syntax as
 * given in RFC1034, Section 3.5.
 * @return 0 on success.
 */
PN_TLS_EXTERN int pn_tls_set_peer_hostname(pn_tls_t *tls, const char *hostname);

/**
 * Access the configured peer identity.
 *
 * Return the expected identity of the remote peer, as set by ::pn_tls_set_peer_hostname.
 *
 * @param[in] tls the tls session.
 * @param[out] hostname buffer to hold the null-terminated name string. If null, no string
 * is written.
 * @param[in,out] bufsize on input set to the number of octets in hostname. On output, set
 * to the number of octets needed to hold the value of hostname plus a null byte.  Zero if
 * no hostname set.
 * @return 0 on success.
 */
PN_TLS_EXTERN int pn_tls_get_peer_hostname(pn_tls_t *tls, char *hostname, size_t *bufsize);

/**
 * Get the subject from the peers certificate.
 *
 * @param[in] tls the tls client/server to query.
 * @return A null terminated string representing the full subject,
 * which is valid until the tls object is destroyed.
 */
PN_TLS_EXTERN const char* pn_tls_get_remote_subject(pn_tls_t *tls);

/**
 * Enumeration identifying the sub fields of the subject field in the tls certificate.
 */
typedef enum {
  PN_TLS_CERT_SUBJECT_COUNTRY_NAME,
  PN_TLS_CERT_SUBJECT_STATE_OR_PROVINCE,
  PN_TLS_CERT_SUBJECT_CITY_OR_LOCALITY,
  PN_TLS_CERT_SUBJECT_ORGANIZATION_NAME,
  PN_TLS_CERT_SUBJECT_ORGANIZATION_UNIT,
  PN_TLS_CERT_SUBJECT_COMMON_NAME
} pn_tls_cert_subject_subfield;

/**
 * Enumeration identifying hashing algorithm.
 */
typedef enum {
  PN_TLS_SHA1,   /* Produces hash that is 20 bytes long */
  PN_TLS_SHA256, /* Produces hash that is 32 bytes long */
  PN_TLS_SHA512, /* Produces hash that is 64 bytes long */
  PN_TLS_MD5     /* Produces hash that is 16 bytes long */
} pn_tls_hash_alg;

/**
 * Get the fingerprint of the certificate. The certificate fingerprint (as displayed in the Fingerprints section when
 * looking at a certificate with say the Firefox browser) is the hexadecimal hash of the entire certificate.
 * The fingerprint is not part of the certificate, rather it is computed from the certificate and can be used to uniquely identify a certificate.
 * @param[in] tls0 the tls client/server to query
 * @param[in] fingerprint char pointer. The certificate fingerprint (in hex format) will be populated in this array.
 *            If sha1 is the digest name, the fingerprint is 41 characters long (40 + 1 '\0' character), 65 characters long for
 *            sha256 and 129 characters long for sha512 and 33 characters for md5.
 * @param[in] fingerprint_length - Must be at >= 33 for md5, >= 41 for sha1, >= 65 for sha256 and >=129 for sha512.
 * @param[in] hash_alg the hash algorithm to use. Must be of type pn_tls_hash_alg (currently supports sha1, sha256, sha512 and md5)
 * @return error code - Returns 0 on success. Return a value less than zero if there were any errors. Upon execution of this function,
 *                      char *fingerprint will contain the appropriate null terminated hex fingerprint
 */
PN_TLS_EXTERN int pn_tls_get_cert_fingerprint(pn_tls_t *tls0,
                                          char *fingerprint,
                                          size_t fingerprint_length,
                                          pn_tls_hash_alg hash_alg);

/**
 * Returns a char pointer that contains the value of the sub field of the subject field in the tls certificate. The subject field usually contains the following sub fields -
 * C = ISO3166 two character country code
 * ST = state or province
 * L = Locality; generally means city
 * O = Organization - Company Name
 * OU = Organization Unit - division or unit
 * CN = CommonName
 * @param[in] tls0 the tls client/server to query
 * @param[in] field The enumeration pn_tls_cert_subject_subfield representing the required sub field.
 * @return A null terminated string which contains the requested sub field value which is valid until the tls object is destroyed.
 */
PN_TLS_EXTERN const char* pn_tls_get_remote_subject_subfield(pn_tls_t *tls, pn_tls_cert_subject_subfield field);

PN_TLS_EXTERN bool pn_tls_is_encrypt_output_pending(pn_tls_t *tls);
PN_TLS_EXTERN bool pn_tls_is_decrypt_output_pending(pn_tls_t *tls);


// True if peers have negotiated a TLS session.  False indicates handshake in progress or protocol error.
PN_TLS_EXTERN bool pn_tls_is_secure(pn_tls_t *tls);


// ----------------------------------------------------------------------------------------------------------------------

// A common pool of buffers to put both encrypted and decrypted bytes in:
// - this could easily just be split into 2 result buffer sets if preferred.

// Give buffers to store encryption/decryption results
// returns the number of buffers taken - it's possible that we don't have space
// to record all of them
PN_TLS_EXTERN size_t pn_tls_give_encrypt_output_buffers(pn_tls_t*, pn_raw_buffer_t const*, size_t count);
PN_TLS_EXTERN size_t pn_tls_give_decrypt_output_buffers(pn_tls_t*, pn_raw_buffer_t const*, size_t count);


// Take result buffers back into app ownership, return the actual number of buffers returned
// keep calling these until the number returned is 0 to make sure you get all buffers currently available
// Gives only buffers with encrypted/decrypted content before pn_tls_stop() and all buffers afterwards.
PN_TLS_EXTERN size_t pn_tls_take_decrypt_output_buffers(pn_tls_t*, pn_raw_buffer_t*, size_t count);
PN_TLS_EXTERN size_t pn_tls_take_encrypt_output_buffers(pn_tls_t*, pn_raw_buffer_t*, size_t count);

// Stage data to be encrypted by the engine at a future pn_tls_process() step.
// returned value is number of buffers taken (ownership transfer)
// i.e. held by the tls code - governed by capacity.
PN_TLS_EXTERN size_t pn_tls_give_encrypt_input_buffers(pn_tls_t*, pn_raw_buffer_t const* bufs, size_t count_bufs);

// returned value is number of buffers taken
PN_TLS_EXTERN size_t pn_tls_give_decrypt_input_buffers(pn_tls_t*, pn_raw_buffer_t const* bufs, size_t count_bufs);

// Take input buffers back into app ownership, return the actual number of buffers returned
// Returns buffers whose data is fully processed (or pn_tls_stop() called).
// keep calling these until the number returned is 0 to make sure you get all buffers currently available
PN_TLS_EXTERN size_t pn_tls_take_encrypt_input_buffers(pn_tls_t*, pn_raw_buffer_t*, size_t count);
PN_TLS_EXTERN size_t pn_tls_take_decrypt_input_buffers(pn_tls_t*, pn_raw_buffer_t*, size_t count);

// Return the max number of additional input buffers we can hold
PN_TLS_EXTERN size_t pn_tls_get_encrypt_input_buffer_capacity(pn_tls_t*);
PN_TLS_EXTERN size_t pn_tls_get_decrypt_input_buffer_capacity(pn_tls_t*);

// True if there is no remaining space in the XXcrypt result buffers owned by the
// engine and there is XXcryped data available to put in a result buffer since
// the last pn_tls_process()
PN_TLS_EXTERN bool pn_tls_need_encrypt_output_buffers(pn_tls_t*);
PN_TLS_EXTERN bool pn_tls_need_decrypt_output_buffers(pn_tls_t*);

PN_TLS_EXTERN size_t pn_tls_get_encrypt_output_buffer_capacity(pn_tls_t*);
PN_TLS_EXTERN size_t pn_tls_get_decrypt_output_buffer_capacity(pn_tls_t*);

// Number of buffers ready to be returned by take operation since last pn_tls_process() or pn_tls_stop()
PN_TLS_EXTERN size_t pn_tls_get_decrypt_output_buffer_count(pn_tls_t*);
PN_TLS_EXTERN size_t pn_tls_get_encrypt_output_buffer_count(pn_tls_t*);

PN_TLS_EXTERN uint32_t pn_tls_get_last_decrypt_output_buffer_size(pn_tls_t*);
PN_TLS_EXTERN uint32_t pn_tls_get_last_encrypt_output_buffer_size(pn_tls_t*);


// Configurable.  Zero implies "use default".  No effect after pn_tls_start().
PN_TLS_EXTERN void pn_tls_set_encrypt_input_buffer_max_capacity(pn_tls_t*, size_t s);
PN_TLS_EXTERN void pn_tls_set_decrypt_input_buffer_max_capacity(pn_tls_t*, size_t s);
PN_TLS_EXTERN void pn_tls_set_encrypt_output_buffer_max_capacity(pn_tls_t*, size_t s);
PN_TLS_EXTERN void pn_tls_set_decrypt_output_buffer_max_capacity(pn_tls_t*, size_t s);

// Process as much unencrypted data to encrypted result data as possible.
// Also process as much undecrypted data to decryptedcrypted result data as possible.
// Check for TLS engine errors here.
// Always returns an error if called before pn_tls_start() or after pn_tls_stop().
PN_TLS_EXTERN int pn_tls_process(pn_tls_t* tls);

// Future pn_tls_process() or pn_tls_give_xxx() are no-ops.
// Unused encrypt/decrypt result buffers become zero length encrypted/decrypted result
// buffers and can be reclaimed.
// Future: If no closure from peer or self closure not written to result buffer, session resume cancelled.
PN_TLS_EXTERN int pn_tls_stop(pn_tls_t* tls);

// Confirms receipt of the peer's TLS closure record.  This confirms clean shutdown and
// the absence of a truncation attack.
PN_TLS_EXTERN bool pn_tls_is_input_closed(pn_tls_t* tls);

// Closes the encrypt side and appends the TLS closure record to the pending encypted
// output.  pn_tls_give_encrypt_input_buffers() will no longer take any supplied buffers.
PN_TLS_EXTERN void pn_tls_close_output(pn_tls_t* tls);

// If non-zero the TLS session was unable to start or was aborted.
// The application should stop all read activity, and take all
// remaining encrypted content and write it onto the connection
// (i.e. until pn_tls_is_encrypt_output_pending() is false), then
// close the associated connection.  This will allow the error to be
// propagated to the peer if expected by the TLS protocol.  Specific return values TBD (INIT_FAILED,
// BAD_AUTH, TLS_PROTOCOL_ERROR, ...).
PN_TLS_EXTERN int pn_tls_get_session_error(pn_tls_t* tls);

// Error string associated with the fatal TLS session error. zero if no error, buf is null or buf_len is 0.
PN_TLS_EXTERN size_t pn_tls_get_session_error_string(pn_tls_t* tls, char *buf, size_t buf_len);

/**
 * Provide an ordered list of application protols for RFC 7301 negotiation.
 * List ordered in descending preference for the caller.
 *
 * Each protocol name must be provided as its well known octet sequence in UTF-8, null
 * terminated.  For the client, the order is preserved in the client_hello to the server, but
 * the server will not usually take that ordering into account.
 *
 * For the server, protocol selection follows the standard mechanism in RFC 7301: the first item in the server list that matches an item in the client list is selected.  No match will result in a failed TLS handshake.
 *
 * ALPN processing can be turned off by setting protocols to NULL and protocol_count to zero.
 *
 * @note This setting effects only those pn_tls_t objects created after this call
 * returns.  pn_tls_t objects created before invoking this method will use the domain's
 * previous setting.
 *
 * @param[in] tls the tls client/server to query.
 * @param[in] protocols the array of pointers the protocol names in the list.
 * @param[in] count the size of the protocols array.
 * @return 0 on success, PN_ARG_ERROR if any array pointers are null or any protocol names exceed 255 bytes in length. PN_OUT_OF_MEMORY if memory allocation fails.
*/
PN_TLS_EXTERN int pn_tls_config_set_alpn_protocols(pn_tls_config_t *domain, const char **protocols, size_t protocol_count);

/**
 * Get the name of the negotiated application protocol.
 *
 * Gets the name of the negotiated application protocol or returns FALSE if the negotiation
 * failed, is not yet complete, or was never requested by the client.  The protocol name is not
 * a null terminated string.
 *
 * @param[in] tls the tls client/server to query.
 * @param[out] protocol_name set to a pointer to the application protocol name or NULL if not negotiated.
 * @param[out] size set to the length of the protocol name or zero if not negotiated.
 * @return True if the protocol_name is non-null and size is not zero.
 */
PN_TLS_EXTERN bool pn_tls_get_alpn_protocol(pn_tls_t *tls, const char **protocol_name, size_t *size);

// TODO:  Tracing.  Session tickets.

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* tls.h */
