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
 *
 */
package org.apache.qpid.proton.engine.jni;

import static org.apache.qpid.proton.jni.ExceptionHelper.checkProtonCReturnValue;

import org.apache.qpid.proton.ProtonCEquivalent;
import org.apache.qpid.proton.ProtonUnsupportedOperationException;
import org.apache.qpid.proton.engine.SslDomain;
import org.apache.qpid.proton.jni.Proton;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_ssl_domain_t;
import org.apache.qpid.proton.jni.pn_ssl_mode_t;
import org.apache.qpid.proton.jni.pn_ssl_verify_mode_t;

public class JNISslDomain implements SslDomain
{
    private SWIGTYPE_p_pn_ssl_domain_t _impl;
    private Mode _mode;
    private VerifyMode _verifyMode;
    private String _certificateFile;
    private String _trustedCaDb;
    private String _privateKeyFile;
    private String _privateKeyPassword;
    private boolean _allowUnsecured;

    @Override
    @ProtonCEquivalent("pn_ssl_init")
    public void init(Mode mode)
    {
        _impl = Proton.pn_ssl_domain(convertMode(mode));
        if (_impl == null)
            throw new ProtonUnsupportedOperationException("No SSL libraries available.");
    }

    private pn_ssl_mode_t convertMode(Mode mode)
    {
        _mode = mode;
        pn_ssl_mode_t cMode = null;
        switch(mode)
        {
            case CLIENT:
                cMode = pn_ssl_mode_t.PN_SSL_MODE_CLIENT;
                break;
            case SERVER:
                cMode = pn_ssl_mode_t.PN_SSL_MODE_SERVER;
                break;
        }
        return cMode;
    }

    @Override
    public Mode getMode()
    {
        return _mode;
    }

    @Override
    @ProtonCEquivalent("pn_ssl_domain_set_credentials")
    public void setCredentials(String certificate_file, String private_key_file, String password)
    {
        _certificateFile = certificate_file;
        _privateKeyFile = private_key_file;
        _privateKeyPassword = password;
        int retVal = Proton.pn_ssl_domain_set_credentials(_impl,certificate_file,private_key_file,password);
        checkProtonCReturnValue(retVal);
    }

    @Override
    public String getPrivateKeyFile()
    {
        return _privateKeyFile;
    }

    @Override
    public String getPrivateKeyPassword()
    {
        return _privateKeyPassword;
    }

    @Override
    public String getCertificateFile()
    {
        return _certificateFile;
    }


    @Override
    @ProtonCEquivalent("pn_ssl_domain_set_trusted_ca_db")
    public void setTrustedCaDb(String certificate_db)
    {
        _trustedCaDb = certificate_db;
        int retVal = Proton.pn_ssl_domain_set_trusted_ca_db(_impl, certificate_db);
        checkProtonCReturnValue(retVal);
    }

    @Override
    public String getTrustedCaDb()
    {
        return _trustedCaDb;
    }

    @Override
    @ProtonCEquivalent("pn_ssl_domain_allow_unsecured_client")
    public void allowUnsecuredClient(boolean unused)
    {
        _allowUnsecured = true;
        int retVal = Proton.pn_ssl_domain_allow_unsecured_client(_impl);
        checkProtonCReturnValue(retVal);
    }

    @Override
    public boolean allowUnsecuredClient()
    {
        return _allowUnsecured;
    }

    @Override
    @ProtonCEquivalent("pn_ssl_domain_set_peer_authentication")
    public void setPeerAuthentication(VerifyMode mode)
    {
        _verifyMode = mode;
        int retVal = Proton.pn_ssl_domain_set_peer_authentication(_impl,convertVerifyMode(mode),_trustedCaDb);
        checkProtonCReturnValue(retVal);
    }

    private pn_ssl_verify_mode_t convertVerifyMode(VerifyMode mode)
    {
        pn_ssl_verify_mode_t cMode = null;
        switch(mode)
        {
            case ANONYMOUS_PEER:
                cMode = pn_ssl_verify_mode_t.PN_SSL_ANONYMOUS_PEER;
                break;
            case VERIFY_PEER:
                cMode = pn_ssl_verify_mode_t.PN_SSL_VERIFY_PEER;
                break;
            case VERIFY_PEER_NAME:
                cMode = pn_ssl_verify_mode_t.PN_SSL_VERIFY_PEER_NAME;
                break;
            default:
                throw new IllegalArgumentException("Unsupported verify mode " + mode);
        }

        return cMode;
    }

    @Override
    public VerifyMode getPeerAuthentication()
    {
        return _verifyMode;
    }

    SWIGTYPE_p_pn_ssl_domain_t getImpl()
    {
        if (_impl == null)
        {
            throw new IllegalStateException("init has not been called");
        }
        return _impl;
    }
}
