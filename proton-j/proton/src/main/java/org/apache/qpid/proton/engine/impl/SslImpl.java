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
package org.apache.qpid.proton.engine.impl;

import org.apache.qpid.proton.engine.Ssl;
import org.apache.qpid.proton.engine.TransportInput;
import org.apache.qpid.proton.engine.TransportOutput;
import org.apache.qpid.proton.engine.TransportWrapper;

/*
 * Enables the JSSE system debugging system property:
 *
 *     -Djavax.net.debug=all
 */
public class SslImpl implements Ssl
{
    private Mode _mode;
    private VerifyMode _verifyMode = VerifyMode.ANONYMOUS_PEER;
    private String _certificateFile;
    private String _privateKeyFile;
    private String _privateKeyPassword;
    private String _trustedCaDb;
    private boolean _allowUnsecuredClient;

    private SslTransportWrapper _unsecureClientAwareTransportWrapper;

    @Override
    public void init(Mode mode)
    {
        _mode = mode;
    }

    @Override
    public void setCredentials(String certificateFile,
            String privateKeyFile, String privateKeyPassword)
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
    public void allowUnsecuredClient(boolean allowUnsecuredClient)
    {
        if (_mode == Mode.CLIENT)
        {
            throw new IllegalArgumentException("Only servers may allow unsecured clients");
        }
        _allowUnsecuredClient = allowUnsecuredClient;
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

    TransportWrapper wrap(TransportInput inputProcessor, TransportOutput outputProcessor)
    {
        if (_unsecureClientAwareTransportWrapper != null)
        {
            throw new IllegalStateException("Transport already wrapped");
        }

        _unsecureClientAwareTransportWrapper = new UnsecureClientAwareTransportWrapper(inputProcessor, outputProcessor);
        return _unsecureClientAwareTransportWrapper;
    }

    @Override
    public String getCipherName()
    {
        if(_unsecureClientAwareTransportWrapper == null)
        {
            throw new IllegalStateException("Transport wrapper is uninitialised");
        }

        return _unsecureClientAwareTransportWrapper.getCipherName();
    }

    @Override
    public String getProtocolName()
    {
        if(_unsecureClientAwareTransportWrapper == null)
        {
            throw new IllegalStateException("Transport wrapper is uninitialised");
        }

        return _unsecureClientAwareTransportWrapper.getProtocolName();
    }

    private class UnsecureClientAwareTransportWrapper implements SslTransportWrapper
    {
        private final TransportInput _inputProcessor;
        private final TransportOutput _outputProcessor;
        private SslTransportWrapper _transportWrapper;

        private UnsecureClientAwareTransportWrapper(TransportInput inputProcessor,
                TransportOutput outputProcessor)
        {
            _inputProcessor = inputProcessor;
            _outputProcessor = outputProcessor;
        }

        @Override
        public int input(byte[] bytes, int offset, int size)
        {
            initTransportWrapperOnFirstIO();
            return _transportWrapper.input(bytes, offset, size);
        }

        @Override
        public int output(byte[] bytes, int offset, int size)
        {
            initTransportWrapperOnFirstIO();
            return _transportWrapper.output(bytes, offset, size);
        }

        @Override
        public String getCipherName()
        {
            if (_transportWrapper == null)
            {
                return null;
            }
            else
            {
                return _transportWrapper.getCipherName();
            }
        }

        @Override
        public String getProtocolName()
        {
            if(_transportWrapper == null)
            {
                return null;
            }
            else
            {
                return _transportWrapper.getProtocolName();
            }
        }

        private void initTransportWrapperOnFirstIO()
        {
            if (_transportWrapper == null)
            {
                SslTransportWrapper sslTransportWrapper = new SimpleSslTransportWrapper(SslImpl.this, _inputProcessor, _outputProcessor);
                if (_allowUnsecuredClient)
                {
                    TransportWrapper plainTransportWrapper = new PlainTransportWrapper(_outputProcessor, _inputProcessor);
                    _transportWrapper = new SslHandshakeSniffingTransportWrapper(sslTransportWrapper, plainTransportWrapper);
                }
                else
                {
                    _transportWrapper = sslTransportWrapper;
                }
            }
        }
    }
}
