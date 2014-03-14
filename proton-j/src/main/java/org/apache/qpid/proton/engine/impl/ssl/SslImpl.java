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
package org.apache.qpid.proton.engine.impl.ssl;

import java.nio.ByteBuffer;

import org.apache.qpid.proton.ProtonUnsupportedOperationException;
import org.apache.qpid.proton.engine.Ssl;
import org.apache.qpid.proton.engine.SslDomain;
import org.apache.qpid.proton.engine.SslPeerDetails;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.engine.TransportException;
import org.apache.qpid.proton.engine.impl.TransportInput;
import org.apache.qpid.proton.engine.impl.TransportOutput;
import org.apache.qpid.proton.engine.impl.TransportWrapper;
import org.apache.qpid.proton.engine.impl.PlainTransportWrapper;

public class SslImpl implements Ssl
{
    private SslTransportWrapper _unsecureClientAwareTransportWrapper;

    private final SslDomain _domain;
    private final ProtonSslEngineProvider _protonSslEngineProvider;

    private final SslPeerDetails _peerDetails;
    private TransportException _initException;

    /**
     * @param sslDomain must implement {@link ProtonSslEngineProvider}. This is not possible
     * enforce at the API level because {@link ProtonSslEngineProvider} is not part of the
     * public Proton API.</p>
     */
    public SslImpl(SslDomain domain, SslPeerDetails peerDetails)
    {
        _domain = domain;
        _protonSslEngineProvider = (ProtonSslEngineProvider)domain;
        _peerDetails = peerDetails;
    }

    public TransportWrapper wrap(TransportInput inputProcessor, TransportOutput outputProcessor)
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
        public int capacity()
        {
            initTransportWrapperOnFirstIO();
            if (_initException == null) {
                return _transportWrapper.capacity();
            } else {
                return Transport.END_OF_STREAM;
            }
        }

        @Override
        public ByteBuffer tail()
        {
            initTransportWrapperOnFirstIO();
            if (_initException == null) {
                return _transportWrapper.tail();
            } else {
                return null;
            }
        }


        @Override
        public void process() throws TransportException
        {
            initTransportWrapperOnFirstIO();
            if (_initException == null) {
                _transportWrapper.process();
            } else {
                throw new TransportException(_initException);
            }
        }

        @Override
        public void close_tail()
        {
            initTransportWrapperOnFirstIO();
            if (_initException == null) {
                _transportWrapper.close_tail();
            }
        }

        @Override
        public int pending()
        {
            initTransportWrapperOnFirstIO();
            if (_initException == null) {
                return _transportWrapper.pending();
            } else {
                throw new TransportException(_initException);
            }
        }

        @Override
        public ByteBuffer head()
        {
            initTransportWrapperOnFirstIO();
            if (_initException == null) {
                return _transportWrapper.head();
            } else {
                return null;
            }
        }

        @Override
        public void pop(int bytes)
        {
            initTransportWrapperOnFirstIO();
            if (_initException == null) {
                _transportWrapper.pop(bytes);
            }
        }

        @Override
        public void close_head()
        {
            initTransportWrapperOnFirstIO();
            if (_initException == null) {
                _transportWrapper.close_head();
            }
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
            try {
                if (_initException == null && _transportWrapper == null)
                {
                    SslTransportWrapper sslTransportWrapper = new SimpleSslTransportWrapper
                        (_protonSslEngineProvider.createSslEngine(_peerDetails),
                         _inputProcessor, _outputProcessor);

                    if (_domain.allowUnsecuredClient() && _domain.getMode() == SslDomain.Mode.SERVER)
                    {
                        TransportWrapper plainTransportWrapper = new PlainTransportWrapper
                            (_outputProcessor, _inputProcessor);
                        _transportWrapper = new SslHandshakeSniffingTransportWrapper
                            (sslTransportWrapper, plainTransportWrapper);
                    }
                    else
                    {
                        _transportWrapper = sslTransportWrapper;
                    }
                }
            } catch (TransportException e) {
                _initException = e;
            }
        }
    }

    /**
     * {@inheritDoc}
     * @throws ProtonUnsupportedOperationException
     */
    @Override
    public void setPeerHostname(String hostname)
    {
        throw new ProtonUnsupportedOperationException();
    }

    /**
     * {@inheritDoc}
     * @throws ProtonUnsupportedOperationException
     */
    @Override
    public String getPeerHostname()
    {
        throw new ProtonUnsupportedOperationException();
    }
}
