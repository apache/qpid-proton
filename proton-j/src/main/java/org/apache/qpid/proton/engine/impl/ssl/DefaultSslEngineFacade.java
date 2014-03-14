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
package org.apache.qpid.proton.engine.impl.ssl;

import java.nio.ByteBuffer;

import javax.net.ssl.SSLEngine;
import javax.net.ssl.SSLEngineResult;
import javax.net.ssl.SSLEngineResult.HandshakeStatus;
import javax.net.ssl.SSLEngineResult.Status;
import javax.net.ssl.SSLException;
import javax.net.ssl.SSLSession;


class DefaultSslEngineFacade implements ProtonSslEngine
{
    private final SSLEngine _sslEngine;

    /**
     * Our testing has shown that application buffers need to be a bit larger
     * than that provided by {@link SSLSession#getApplicationBufferSize()} otherwise
     * {@link Status#BUFFER_OVERFLOW} will result on {@link SSLEngine#unwrap()}.
     * Sun's own example uses 50, so we use the same.
     */
    private static final int APPLICATION_BUFFER_EXTRA = 50;

    DefaultSslEngineFacade(SSLEngine sslEngine)
    {
        _sslEngine = sslEngine;
    }

    @Override
    public SSLEngineResult wrap(ByteBuffer src, ByteBuffer dst) throws SSLException
    {
        return _sslEngine.wrap(src, dst);
    }

    @Override
    public SSLEngineResult unwrap(ByteBuffer src, ByteBuffer dst) throws SSLException
    {
        return _sslEngine.unwrap(src, dst);
    }

    /**
     * @see #APPLICATION_BUFFER_EXTRA
     */
    @Override
    public int getEffectiveApplicationBufferSize()
    {
        return getApplicationBufferSize() + APPLICATION_BUFFER_EXTRA;
    }

    private int getApplicationBufferSize()
    {
        return _sslEngine.getSession().getApplicationBufferSize();
    }

    @Override
    public int getPacketBufferSize()
    {
        return _sslEngine.getSession().getPacketBufferSize();
    }

    @Override
    public String getCipherSuite()
    {
        return _sslEngine.getSession().getCipherSuite();
    }

    @Override
    public String getProtocol()
    {
        return _sslEngine.getSession().getProtocol();
    }

    @Override
    public Runnable getDelegatedTask()
    {
        return _sslEngine.getDelegatedTask();
    }

    @Override
    public HandshakeStatus getHandshakeStatus()
    {
        return _sslEngine.getHandshakeStatus();
    }

    @Override
    public boolean getUseClientMode()
    {
        return _sslEngine.getUseClientMode();
    }

    @Override
    public String toString()
    {
        StringBuilder builder = new StringBuilder();
        builder.append("DefaultSslEngineFacade [_sslEngine=").append(_sslEngine).append("]");
        return builder.toString();
    }

}
