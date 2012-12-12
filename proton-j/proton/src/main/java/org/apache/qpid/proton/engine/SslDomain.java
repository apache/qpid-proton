/*
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
 */
package org.apache.qpid.proton.engine;

import org.apache.qpid.proton.engine.Ssl.Mode;
import org.apache.qpid.proton.engine.Ssl.VerifyMode;
import org.apache.qpid.proton.engine.impl.ssl.SslEngineFacade;

/**
 * PHTODO
 */
public interface SslDomain
{
    Mode getMode();

    /**
     * Set the certificate that identifies the local node to the remote.
     *
     * This certificate establishes the identity for the local node. It will be sent to the
     * remote if the remote needs to verify the identity of this node. This may be used for
     * both SSL servers and SSL clients (if client authentication is required by the server).
     *
     * @param certificate_file path to file/database containing the identifying
     * certificate.
     * @param private_key_file path to file/database containing the private key used to
     * sign the certificate
     * @param password the password used to sign the key, else NULL if key is not
     * protected.
     */
    void setCredentials(String certificate_file, String private_key_file, String password);

    String getPrivateKeyFile(); // TODO

    String getPrivateKeyPassword();

    String getCertificateFile();

    /**
     * Configure the set of trusted CA certificates used by this node to verify peers.
     *
     * If the local SSL client/server needs to verify the identity of the remote, it must
     * validate the signature of the remote's certificate. This function sets the database of
     * trusted CAs that will be used to verify the signature of the remote's certificate.
     *
     * @param certificate_db database of trusted CAs, used to authenticate the peer.
     */
    void setTrustedCaDb(String certificate_db);

    String getTrustedCaDb();

    /**
     * Configure the level of verification used on the peer certificate.
     *
     * This method controls how the peer's certificate is validated, if at all. By default,
     * neither servers nor clients attempt to verify their peers (PN_SSL_ANONYMOUS_PEER).
     * Once certificates and trusted CAs are configured, peer verification can be enabled.
     *
     * In order to verify a peer, a trusted CA must be configured. See
     * #setTrustedCaDb().
     *
     * @note Servers must provide their own certificate when verifying a peer. See
     * #setCredentials().
     *
     * @param mode the level of validation to apply to the peer
     *
     * PHTODO rename to setDefaultPeerAuthentication?
     */
    void setPeerAuthentication(VerifyMode mode);

    VerifyMode getPeerAuthentication();

    SslEngineFacade getSslEngineFacade(SslPeerDetails sslPeerDetails);
}
