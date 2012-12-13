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

package org.apache.qpid.proton.engine.jni;

import java.nio.ByteBuffer;
import java.nio.charset.Charset;

import org.apache.qpid.proton.engine.Ssl;
import org.apache.qpid.proton.jni.Proton;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_ssl_t;
import org.apache.qpid.proton.jni.pn_ssl_mode_t;
import org.apache.qpid.proton.jni.pn_ssl_verify_mode_t;

class JNISsl implements Ssl
{
    private final SWIGTYPE_p_pn_ssl_t _impl;
    private Mode _mode;
    private String _certificateFile;
    private String _privateKeyFile;
    private String _privateKeyPassword;
    private VerifyMode _verifyMode;

    JNISsl(SWIGTYPE_p_pn_ssl_t impl)
    {
        _impl = impl;
    }

    @Override
    public void init(Mode mode)
    {
        Proton.pn_ssl_init(_impl, convertMode(mode));

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
    public void setCredentials(String certificate_file, String private_key_file, String password)
    {
        _certificateFile = certificate_file;
        _privateKeyFile = private_key_file;
        _privateKeyPassword = password;
        Proton.pn_ssl_set_credentials(_impl,certificate_file,private_key_file,password);

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
    public void setTrustedCaDb(String certificate_db)
    {
        Proton.pn_ssl_set_trusted_ca_db(_impl, certificate_db);

    }

    @Override
    public String getTrustedCaDb()
    {
        return _certificateFile;
    }

    @Override
    public void allowUnsecuredClient(boolean allowUnsecured)
    {
        Proton.pn_ssl_allow_unsecured_client(_impl);

    }

    @Override
    public void setPeerAuthentication(VerifyMode mode)
    {
        _verifyMode = mode;
        Proton.pn_ssl_set_peer_authentication(_impl,convertVerifyMode(mode),_certificateFile);

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
        }

        return cMode;
    }

    @Override
    public VerifyMode getPeerAuthentication()
    {
        return _verifyMode;
    }

    @Override
    public String getCipherName()
    {
        byte[] data = new byte[1024];
        boolean b = Proton.pn_ssl_get_cipher_name(_impl, ByteBuffer.wrap(data));
        return b ? asString(data) : null;

    }

    private String asString(byte[] data)
    {
        int i = -1;
        while(data[++i] != 0);
        return new String(data,0,i,Charset.forName("US-ASCII"));
    }

    @Override
    public String getProtocolName()
    {
        byte[] data = new byte[1024];
        boolean b = Proton.pn_ssl_get_protocol_name(_impl, ByteBuffer.wrap(data));
        return b ? asString(data) : null;
    }
}
