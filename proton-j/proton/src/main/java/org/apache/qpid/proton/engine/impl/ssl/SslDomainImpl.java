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
package org.apache.qpid.proton.engine.impl.ssl;

import org.apache.qpid.proton.engine.Ssl.Mode;
import org.apache.qpid.proton.engine.Ssl.VerifyMode;
import org.apache.qpid.proton.engine.SslDomain;
import org.apache.qpid.proton.engine.SslPeerDetails;

public class SslDomainImpl implements SslDomain
{
    private Mode _mode;
    private VerifyMode _verifyMode = VerifyMode.ANONYMOUS_PEER;
    private String _certificateFile;
    private String _privateKeyFile;
    private String _privateKeyPassword;
    private String _trustedCaDb;

    private final SslEngineFacadeFactory _sslEngineFacadeFactory = new SslEngineFacadeFactory();

    public void setMode(Mode mode)
    {
        _mode = mode;
    }

    @Override
    public void setCredentials(String certificateFile, String privateKeyFile, String privateKeyPassword)
    {
        _certificateFile = certificateFile;
        _privateKeyFile = privateKeyFile;
        _privateKeyPassword = privateKeyPassword;
    }

    @Override
    public void setTrustedCaDb(String certificateDb)
    {
        _trustedCaDb = certificateDb;
    }

    @Override
    public String getTrustedCaDb()
    {
        return _trustedCaDb;
    }

    @Override
    public void setPeerAuthentication(VerifyMode verifyMode)
    {
        _verifyMode = verifyMode;
    }

    @Override
    public VerifyMode getPeerAuthentication()
    {
        return _verifyMode;
    }

    @Override
    public Mode getMode()
    {
        return _mode;
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
    public SslEngineFacade getSslEngineFacade(SslPeerDetails peerDetails)
    {
        return _sslEngineFacadeFactory.createSslEngineFacade(this, peerDetails);
    }

}
