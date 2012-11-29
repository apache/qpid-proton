package org.apache.qpid.proton.engine;
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

public interface Ssl
{
    public enum Mode
    {
        CLIENT,    /** Local connection endpoint is an SSL client */
        SERVER     /** Local connection endpoint is an SSL server */
    }


    /** Initialize the pn_ssl_t object.
     *
     * An SSL object be either an SSL server or an SSL client.  It cannot be both. Those
     * transports that will be used to accept incoming connection requests must be configured
     * as an SSL server. Those transports that will be used to initiate outbound connections
     * must be configured as an SSL client.
     *
     */
    void init(Mode mode);

    Mode getMode();

    /** Set the certificate that identifies the local node to the remote.
     *
     * This certificate establishes the identity for the local node.  It will be sent to the
     * remote if the remote needs to verify the identity of this node.  This may be used for
     * both SSL servers and SSL clients (if client authentication is required by the server).
     *
     * @param certificate_file path to file/database containing the identifying
     * certificate.
     * @param private_key_file path to file/database containing the private key used to
     * sign the certificate
     * @param password the password used to sign the key, else NULL if key is not
     * protected.
     */
     void setCredentials( String certificate_file,
                         String private_key_file,
                         String password);

     String getPrivateKeyFile(); // TODO
     String getPrivateKeyPassword();

     String getCertificateFile();

    /** Configure the set of trusted CA certificates used by this node to verify peers.
     *
     * If the local SSL client/server needs to verify the identity of the remote, it must
     * validate the signature of the remote's certificate.  This function sets the database of
     * trusted CAs that will be used to verify the signature of the remote's certificate.
     *
     * @param certificate_db database of trusted CAs, used to authenticate the peer.
     */

    void setTrustedCaDb(String certificate_db);

    String getTrustedCaDb();

    /** Permit a server to accept connection requests from non-SSL clients.
     *
     * This configures the server to "sniff" the incoming client data stream, and dynamically
     * determine whether SSL/TLS is being used.  This option is disabled by default: only
     * clients using SSL/TLS are accepted.
     *
     */
    void allowUnsecuredClient(boolean allowUnsecured);

    /** Determines the level of peer validation.
     *
     *  VERIFY_PEER will only connect to those peers that provide a valid identifying
     *  certificate signed by a trusted CA and are using an authenticated cipher.
     *  ANONYMOUS_PEER does not require a valid certificate, and permits use of ciphers that
     *  do not provide authentication.
     *
     *  ANONYMOUS_PEER is configured by default.
     *
     *  These settings can be changed via ::pn_ssl_set_peer_authentication()
     */
    public enum VerifyMode
    {
      VERIFY_PEER,     /** require peer to provide a valid identifying certificate */
      ANONYMOUS_PEER,  /** do not require a certificate nor cipher authorization */
    }


    /** Configure the level of verification used on the peer certificate.
     *
     * This method controls how the peer's certificate is validated, if at all.  By default,
     * neither servers nor clients attempt to verify their peers (PN_SSL_ANONYMOUS_PEER).
     * Once certificates and trusted CAs are configured, peer verification can be enabled.
     *
     * In order to verify a peer, a trusted CA must be configured. See
     * #setTrustedCaDb().
     *
     * @note Servers must provide their own certificate when verifying a peer.  See
     * #setCredentials().
     *
     * @param mode the level of validation to apply to the peer
     */
    void setPeerAuthentication(VerifyMode mode);

    VerifyMode getPeerAuthentication();

    /** Get the name of the Cipher that is currently in use.
     *
     * Gets a text description of the cipher that is currently active, or returns null if SSL
     * is not active (no cipher).  Note that the cipher in use may change over time due to
     * renegotiation or other changes to the SSL state.
     *
     * @return the name of the cipher in use, or null if none
     */
    String getCipherName();

    /** Get the name of the SSL protocol that is currently in use.
     *
     * Gets a text description of the SSL protocol that is currently active, or null if SSL
     * is not active.  Note that the protocol may change over time due to renegotiation.
     *
     * @return the name of the protocol in use, or null if none
     */
    String getProtocolName();






}
