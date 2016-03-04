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

import org.apache.qpid.proton.engine.WebSocketHandler;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.engine.TransportException;
import org.apache.qpid.proton.engine.WebSocket;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.Map;
import java.util.logging.Level;
import java.util.logging.Logger;

import static org.apache.qpid.proton.engine.impl.ByteBufferUtils.*;

public class WebSocketImpl implements WebSocket
{
    private static final Logger _logger = Logger.getLogger(WebSocketImpl.class.getName());

    private boolean _tail_closed = false;
    private final ByteBuffer _inputBuffer;
    private boolean _head_closed = false;
    private final ByteBuffer _outputBuffer;
    private ByteBuffer _pingBuffer;

    private int _underlyingOutputSize = 0;
    private int _webSocketHeaderSize = 0;

    private WebSocketHandler _webSocketHandler;
    private Boolean _isWebSocketEnabled = false;
    private WebSocketState _state = WebSocketState.PN_WS_NOT_STARTED;

    private String _host = "";
    private String _path = "";
    private int _port = 0;
    private String _protocol = "";
    private Map<String, String> _additionalHeaders = null;

    public WebSocketImpl(int maxFrameSize)
    {
        _inputBuffer = newWriteableBuffer(maxFrameSize);
        _outputBuffer = newWriteableBuffer(maxFrameSize);
        _pingBuffer = newWriteableBuffer(maxFrameSize);
        _isWebSocketEnabled = false;
    }

    public void configure(
            String host,
            String path,
            int port,
            String protocol,
            Map<String, String> additionalHeaders,
            WebSocketHandler webSocketHandler)
    {
        _host = host;
        _path = path;
        _port = port;
        _protocol = protocol;
        _additionalHeaders = additionalHeaders;

        if (webSocketHandler != null) {
            _webSocketHandler = webSocketHandler;
        }
        else
        {
            _webSocketHandler = new WebSocketHandlerImpl();
        }

        _isWebSocketEnabled = true;
    }

    private void writeUpgradeRequest()
    {
        _outputBuffer.clear();
        String request = _webSocketHandler.createUpgradeRequest(_host, _path, _port, _protocol, _additionalHeaders);

        System.out.println("WEBSOCKETIMPL is sending: ");
        System.out.println(request);
        System.out.println("***************************************************");

        _outputBuffer.put(request.getBytes());

        if (_logger.isLoggable(Level.FINER))
        {
            _logger.log(Level.FINER, "Finished writing WebSocket UpgradeRequest. Output Buffer : " + _outputBuffer);
        }
    }

    private void writePong()
    {
        _webSocketHandler.createPong(_pingBuffer, _outputBuffer);

        if (_logger.isLoggable(Level.FINER))
        {
            _logger.log(Level.FINER, "Ping has been received, sending pong. Output Buffer : " + _outputBuffer);
        }
    }

    public TransportWrapper wrap(final TransportInput input, final TransportOutput output)
    {
        return new WebSocketSniffer(new WebSocketTransportWrapper(input, output), new PlainTransportWrapper(output, input))
        {
            protected boolean isDeterminationMade()
            {
                _selectedTransportWrapper = _wrapper1;
                return true;
            }
        };
    }

    @Override
    public void wrapBuffer(ByteBuffer srcBuffer, ByteBuffer dstBuffer) {
        if (_isWebSocketEnabled)
        {
            _webSocketHandler.wrapBuffer(srcBuffer, dstBuffer);
        }
        else
        {
            dstBuffer.put(srcBuffer);
        }
    }

    @Override
    public WebSocketHandler.WebSocketMessageType unwrapBuffer(ByteBuffer buffer) {
        if (_isWebSocketEnabled)
        {
            return _webSocketHandler.unwrapBuffer(buffer, null);
        }
        else
        {
            return WebSocketHandler.WebSocketMessageType.WEB_SOCKET_MESSAGE_TYPE_EMPTY;
        }
    }

    @Override
    public WebSocketState getState()
    {
        return _state;
    }

    @Override
    public String toString()
    {
        StringBuilder builder = new StringBuilder();
        builder
                .append("WebSocketImpl [isWebSocketEnabled=").append(_isWebSocketEnabled)
                .append(", state=").append(_state)
                .append(", protocol=").append(_protocol)
                .append(", host=").append(_host)
                .append(", path=").append(_path)
                .append(", port=").append(_port)
                .append("]");

        return builder.toString();    }

    private class WebSocketTransportWrapper implements TransportWrapper
    {
        private final TransportInput _underlyingInput;
        private final TransportOutput _underlyingOutput;
        private final ByteBuffer _head;

        private WebSocketTransportWrapper(TransportInput input, TransportOutput output)
        {
            _underlyingInput = input;
            _underlyingOutput = output;
            _head = _outputBuffer.asReadOnlyBuffer();
            _head.limit(0);
        }

        private void processInput() throws TransportException
        {
            switch (_state) {
                case PN_WS_NOT_STARTED:
                    break;
                case PN_WS_CONNECTING:
                    if (_webSocketHandler.validateUpgradeReply(_inputBuffer))
                    {
                        _state = WebSocketState.PN_WS_CONNECTED_FLOW;
                    }

                    if (_logger.isLoggable(Level.FINER))
                    {
                        _logger.log(Level.FINER, WebSocketImpl.this + " about to call plain input");
                    }
                    break;
                case PN_WS_CONNECTED_FLOW:
                case PN_WS_CONNECTED_PONG:
                    Boolean isRepeatedUpgradeAccept = false;
                    if (_inputBuffer.remaining() > 4)
                    {
                        byte[] data = new byte[_inputBuffer.remaining()];
                        _inputBuffer.get(data);
                        if ((data[0] == 72) && (data[1] == 84) && (data[2] == 84) && (data[3] == 80))
                        {
                            _inputBuffer.compact();
                            isRepeatedUpgradeAccept = true;
                        }
                    }

                    if (!isRepeatedUpgradeAccept)
                    {
                        _inputBuffer.flip();

                        switch (unwrapBuffer(_inputBuffer))
                        {
                            case WEB_SOCKET_MESSAGE_TYPE_EMPTY:
                            case WEB_SOCKET_MESSAGE_TYPE_AMQP:
                            case WEB_SOCKET_MESSAGE_TYPE_INVALID_MASKED:
                            case WEB_SOCKET_MESSAGE_TYPE_INVALID_LENGTH:
                            case WEB_SOCKET_MESSAGE_TYPE_INVALID:
                                int bytes = pourAll(_inputBuffer, _underlyingInput);
                                if (bytes == Transport.END_OF_STREAM) {
                                    _tail_closed = true;
                                }
                                _inputBuffer.compact();

                                _underlyingInput.process();
                                break;
                            case WEB_SOCKET_MESSAGE_TYPE_PING:
                                _pingBuffer.put(_inputBuffer);
                                _state = WebSocketState.PN_WS_CONNECTED_PONG;
                                break;
                        }
                    }
                    break;
                case PN_WS_CLOSED:
                    break;
                case PN_WS_FAILED:
                    break;
                default:
                    break;
            }
        }

        @Override
        public int capacity()
        {
            if (_isWebSocketEnabled)
            {
                if (_tail_closed)
                {
                    return Transport.END_OF_STREAM;
                }
                else
                {
                    return _inputBuffer.remaining();
                }
            }
            else
            {
                return _underlyingInput.capacity();
            }
        }

        @Override
        public int position()
        {
            if (_isWebSocketEnabled)
            {
                if (_tail_closed)
                {
                    return Transport.END_OF_STREAM;
                }
                else
                {
                    return _inputBuffer.position();
                }
            }
            else
            {
                return _underlyingInput.position();
            }
        }

        @Override
        public ByteBuffer tail()
        {
            if (_isWebSocketEnabled)
            {
                return _inputBuffer;
            }
            else
            {
                return _underlyingInput.tail();
            }
        }

        @Override
        public void process() throws TransportException
        {
            if (_isWebSocketEnabled)
            {
                _inputBuffer.flip();

                switch (_state)
                {
                    case PN_WS_NOT_STARTED:
                        _underlyingInput.process();
                        break;
                    case PN_WS_CONNECTING:
                        try
                        {
                            processInput();
                        } finally
                        {
                            _inputBuffer.compact();
                        }
                        break;
                    case PN_WS_CONNECTED_FLOW:
                        processInput();
                        break;
                    case PN_WS_FAILED:
                        _underlyingInput.process();
                        break;
                    default:
                        _underlyingInput.process();
                }
            }
            else
            {
                _underlyingInput.process();
            }
        }

        @Override
        public void close_tail()
        {
            _tail_closed = true;
            if (_isWebSocketEnabled)
            {
                _head_closed = true;
                _underlyingInput.close_tail();
            }
            else
            {
                _underlyingInput.close_tail();
            }
        }

        @Override
        public int pending()
        {
            if (_isWebSocketEnabled)
            {
                switch (_state)
                {
                    case PN_WS_NOT_STARTED:
                        if (_outputBuffer.position() == 0)
                        {
                            _state = WebSocketState.PN_WS_CONNECTING;

                            writeUpgradeRequest();

                            _head.limit(_outputBuffer.position());

                            if (_head_closed)
                            {
                                _state = WebSocketState.PN_WS_FAILED;
                                return Transport.END_OF_STREAM;
                            }
                            else
                            {
                                return _outputBuffer.position();
                            }
                        }
                        else
                        {
                            return _outputBuffer.position();
                        }
                    case PN_WS_CONNECTING:
                        if (_head_closed && _outputBuffer.position() == 0)
                        {
                            _state = WebSocketState.PN_WS_FAILED;
                            return Transport.END_OF_STREAM;
                        }
                        else
                        {
                            return _outputBuffer.position();
                        }
                    case PN_WS_CONNECTED_FLOW:
                        _underlyingOutputSize = _underlyingOutput.pending();

                        wrapBuffer(_underlyingOutput.head(), _outputBuffer);

                        _webSocketHeaderSize = _outputBuffer.position() - _underlyingOutputSize;

                        _head.limit(_outputBuffer.position());

                        return _outputBuffer.position();
                    case PN_WS_CONNECTED_PONG:
                        _state = WebSocketState.PN_WS_CONNECTED_FLOW;

                        writePong();

                        _head.limit(_outputBuffer.position());

                        if (_head_closed)
                        {
                            _state = WebSocketState.PN_WS_FAILED;
                            return Transport.END_OF_STREAM;
                        }
                        else
                        {
                            return _outputBuffer.position();
                        }
                    case PN_WS_FAILED:
                        return Transport.END_OF_STREAM;
                    default:
                        return 0;
                }
            }
            else
            {
                return _underlyingOutput.pending();
            }
        }

        @Override
        public ByteBuffer head()
        {
            if (_isWebSocketEnabled)
            {
                switch (_state) {
                    case PN_WS_CONNECTING:
                    case PN_WS_CONNECTED_FLOW:
                    case PN_WS_CONNECTED_PONG:
                        return _head;
                    case PN_WS_NOT_STARTED:
                    case PN_WS_CLOSED:
                    case PN_WS_FAILED:
                    default:
                        return _underlyingOutput.head();
                }
            }
            else
            {
                return _underlyingOutput.head();
            }
        }

        @Override
        public void pop(int bytes)
        {
            if (_isWebSocketEnabled)
            {
                switch (_state)
                {
                    case PN_WS_CONNECTING:
                        if (_outputBuffer.position() != 0)
                        {
                            _outputBuffer.flip();
                            _outputBuffer.position(bytes);
                            _outputBuffer.compact();
                            _head.position(0);
                            _head.limit(_outputBuffer.position());
                        }
                        else
                        {
                            _underlyingOutput.pop(bytes);
                        }
                        break;
                    case PN_WS_CONNECTED_FLOW:
                    case PN_WS_CONNECTED_PONG:
                        if (_outputBuffer.position() != 0)
                        {
                            _outputBuffer.flip();
                            _outputBuffer.position(bytes);
                            _outputBuffer.compact();
                            _head.position(0);
                            _head.limit(_outputBuffer.position());
                            _underlyingOutput.pop(bytes - _webSocketHeaderSize);
                            _webSocketHeaderSize = 0;
                        }
                        else
                        {
                            _underlyingOutput.pop(bytes);
                        }
                        break;
                    case PN_WS_NOT_STARTED:
                    case PN_WS_CLOSED:
                    case PN_WS_FAILED:
                        _underlyingOutput.pop(bytes);
                        break;
                }
            }
            else
            {
                _underlyingOutput.pop(bytes);
            }
        }

        @Override
        public void close_head()
        {
            _underlyingOutput.close_head();
        }
    }
}
