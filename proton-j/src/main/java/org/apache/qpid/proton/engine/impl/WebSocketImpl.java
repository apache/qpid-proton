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

import org.apache.qpid.proton.engine.ExternalWebSocketHandler;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.engine.TransportException;
import org.apache.qpid.proton.engine.WebSocket;

import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.util.logging.Level;
import java.util.logging.Logger;

import static org.apache.qpid.proton.engine.impl.ByteBufferUtils.*;

public class WebSocketImpl implements WebSocket
{
    private static final Logger _logger = Logger.getLogger(WebSocketImpl.class.getName());

    private ExternalWebSocketHandler _externalWebSocketHandler;

    private final TransportImpl _transport;

    private final ByteBuffer _inputBuffer;
    private final ByteBuffer _outputBuffer;

    private WebSocketState _state = WebSocketState.PN_WS_NOT_STARTED;

    /**
     * @param maxFrameSize the size of the input and output buffers
     * returned by {@link WebSocketTransportWrapper#getInputBuffer()} and
     * {@link WebSocketTransportWrapper#getOutputBuffer()}.
     */
    WebSocketImpl(TransportImpl transport, int maxFrameSize, ExternalWebSocketHandler externalWebSocketHandler)
    {
        _transport = transport;
        _inputBuffer = newWriteableBuffer(maxFrameSize);
        _outputBuffer = newWriteableBuffer(maxFrameSize);
        _externalWebSocketHandler = externalWebSocketHandler;
    }

    @Override
    public TransportWrapper wrap(final TransportInput input, final TransportOutput output)
    {
        // TODO: Implement function
        return null;
    }

    @Override
    public TransportWrapper unwrap(TransportInput inputProcessor, TransportOutput outputProcessor)
    {
        // TODO: Implement function
        return null;
    }

    @Override
    final public int recv(byte[] bytes, int offset, int size)
    {
        // TODO: Implement function
        return size;
    }

    @Override
    final public int send(byte[] bytes, int offset, int size)
    {
        // TODO: Implement function
        return size;
    }

    @Override
    public int pending()
    {
        // TODO: Implement function
        return 0;
    }

    @Override
    public WebSocketState getState()
    {
        // TODO: Implement function
        return _state;
    }

    @Override
    public String toString()
    {
        // TODO: Implement function
        return "";
    }

    private class WebSocketTransportWrapper implements TransportWrapper
    {
        private final TransportInput _underlyingInput;
        private final TransportOutput _underlyingOutput;
        private final ByteBuffer _head;
        private boolean _outputComplete;

        private WebSocketTransportWrapper(TransportInput input, TransportOutput output)
        {
            _underlyingInput = input;
            _underlyingOutput = output;
            _head = _outputBuffer.asReadOnlyBuffer();
            _head.limit(0);
        }

        private void fillOutputBuffer()
        {
            // TODO: Implement function
        }

        @Override
        public int capacity()
        {
            // TODO: Implement function
            return 0;
        }

        @Override
        public int position()
        {
            // TODO: Implement function
            return 0;
        }

        @Override
        public ByteBuffer tail()
        {
            // TODO: Implement function
            return null;
        }

        @Override
        public void process() throws TransportException
        {
            // TODO: Implement function
        }

        @Override
        public void close_tail()
        {
            // TODO: Implement function
        }

        @Override
        public int pending()
        {
            // TODO: Implement function
            return 0;
        }

        @Override
        public ByteBuffer head()
        {
            // TODO: Implement function
            return null;
        }

        @Override
        public void pop(int bytes)
        {
            // TODO: Implement function
        }

        @Override
        public void close_head()
        {
            // TODO: Implement function
        }
    }
}
