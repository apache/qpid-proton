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


import static org.apache.qpid.proton.engine.impl.ByteBufferUtils.newReadableBuffer;
import static org.apache.qpid.proton.engine.impl.ByteBufferUtils.newWriteableBuffer;
import static org.apache.qpid.proton.engine.impl.ByteBufferUtils.pourAll;

import java.nio.ByteBuffer;
import java.util.logging.Level;
import java.util.logging.Logger;

import javax.net.ssl.SSLEngineResult;
import javax.net.ssl.SSLEngineResult.HandshakeStatus;
import javax.net.ssl.SSLEngineResult.Status;
import javax.net.ssl.SSLException;

import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.engine.TransportException;
import org.apache.qpid.proton.engine.impl.TransportInput;
import org.apache.qpid.proton.engine.impl.TransportOutput;

/**
 * TODO close the SSLEngine when told to, and modify {@link #wrapOutput()} and {@link #unwrapInput()}
 * to respond appropriately thereafter.
 */
public class SimpleSslTransportWrapper implements SslTransportWrapper
{
    private static final Logger _logger = Logger.getLogger(SimpleSslTransportWrapper.class.getName());

    private final ProtonSslEngine _sslEngine;

    private final TransportInput _underlyingInput;
    private final TransportOutput _underlyingOutput;

    private boolean _tail_closed = false;
    private ByteBuffer _inputBuffer;

    private boolean _head_closed = false;
    private ByteBuffer _outputBuffer;
    private ByteBuffer _head;

    /**
     * A buffer for the decoded bytes that will be passed to _underlyingInput.
     * This extra layer of buffering is necessary in case the underlying input's buffer
     * is too small for SSLEngine to ever unwrap into.
     */
    private ByteBuffer _decodedInputBuffer;

    /** could change during the lifetime of the ssl connection owing to renegotiation. */
    private String _cipherName;

    /** could change during the lifetime of the ssl connection owing to renegotiation. */
    private String _protocolName;


    SimpleSslTransportWrapper(ProtonSslEngine sslEngine, TransportInput underlyingInput, TransportOutput underlyingOutput)
    {
        _underlyingInput = underlyingInput;
        _underlyingOutput = underlyingOutput;
        _sslEngine = sslEngine;

        int effectiveAppBufferMax = _sslEngine.getEffectiveApplicationBufferSize();
        int packetSize = _sslEngine.getPacketBufferSize();

        // Input and output buffers need to be large enough to contain one SSL packet,
        // as stated in SSLEngine JavaDoc.
        _inputBuffer = newWriteableBuffer(packetSize);
        _outputBuffer = newWriteableBuffer(packetSize);
        _head = _outputBuffer.asReadOnlyBuffer();
        _head.limit(0);

        _decodedInputBuffer = newWriteableBuffer(effectiveAppBufferMax);

        if(_logger.isLoggable(Level.FINE))
        {
            _logger.fine("Constructed " + this);
        }
    }


    /**
     * Unwraps the contents of {@link #_inputBuffer} and passes it to {@link #_underlyingInput}.
     *
     * Regarding the state of {@link #_inputBuffer}:
     * - On entry, it is assumed to be readable.
     * - On exit, it is still readable and its "remaining" bytes are those that we were unable
     * to unwrap (e.g. if they don't form a whole packet).
     */
    private void unwrapInput() throws SSLException
    {
        while (true) {
            SSLEngineResult result = _sslEngine.unwrap(_inputBuffer, _decodedInputBuffer);
            logEngineClientModeAndResult(result, "input");

            int read = result.bytesProduced();
            Status status = result.getStatus();
            HandshakeStatus hstatus = result.getHandshakeStatus();

            int capacity = _underlyingInput.capacity();
            if (capacity == Transport.END_OF_STREAM) {
                _tail_closed = true;
                if (_decodedInputBuffer.position() > 0) {
                    throw new TransportException("bytes left unconsumed");
                }
            } else {
                ByteBuffer tail = _underlyingInput.tail();
                _decodedInputBuffer.flip();
                tail.put(_decodedInputBuffer);
                _decodedInputBuffer.compact();
                _underlyingInput.process();
                capacity = _underlyingInput.capacity();
                if (capacity == Transport.END_OF_STREAM) {
                    _tail_closed = true;
                }
            }

            switch (status) {
            case CLOSED:
                _tail_closed = true;
                break;
            case BUFFER_OVERFLOW:
                {
                    ByteBuffer old = _decodedInputBuffer;
                    _decodedInputBuffer = newWriteableBuffer(old.capacity()*2);
                    old.flip();
                    _decodedInputBuffer.put(old);
                }
                continue;
            case BUFFER_UNDERFLOW:
                if (_tail_closed) {
                    _head_closed = true;
                }
                // wait for more data
                break;
            case OK:
                break;
            }

            switch (hstatus)
            {
            case NEED_WRAP:
                // wait for write to kick in
                break;
            case NEED_TASK:
                runDelegatedTasks(result);
                continue;
            case FINISHED:
                updateCipherAndProtocolName(result);
            case NOT_HANDSHAKING:
            case NEED_UNWRAP:
                if (_inputBuffer.position() > 0 && status == Status.OK) {
                    continue;
                } else {
                    if (_inputBuffer.position() == 0 &&
                        hstatus == HandshakeStatus.NEED_UNWRAP &&
                        _tail_closed) {
                        _head_closed = true;
                    }
                    break;
                }
            }

            break;
        }
    }

    /**
     * Wrap the underlying transport's output, passing it to the output buffer.
     *
     * {@link #_outputBuffer} is assumed to be writeable on entry and is guaranteed to
     * be still writeable on exit.
     */
    private void wrapOutput() throws SSLException
    {
        while (true) {
            int pending = _underlyingOutput.pending();
            if (pending < 0) {
                _head_closed = true;
            }

            ByteBuffer clearOutputBuffer = _underlyingOutput.head();
            SSLEngineResult result = _sslEngine.wrap(clearOutputBuffer, _outputBuffer);
            logEngineClientModeAndResult(result, "output");

            int written = result.bytesConsumed();
            _underlyingOutput.pop(written);
            pending = _underlyingOutput.pending();

            Status status = result.getStatus();
            switch (status) {
            case CLOSED:
                _head_closed = true;
                break;
            case OK:
                break;
            case BUFFER_OVERFLOW:
                ByteBuffer old = _outputBuffer;
                _outputBuffer = newWriteableBuffer(_outputBuffer.capacity()*2);
                _head = _outputBuffer.asReadOnlyBuffer();
                old.flip();
                _outputBuffer.put(old);
                continue;
            case BUFFER_UNDERFLOW:
                throw new IllegalStateException("app buffer underflow");
            }

            HandshakeStatus hstatus = result.getHandshakeStatus();
            switch (hstatus) {
            case NEED_UNWRAP:
                // wait for input data
                if (_inputBuffer.position() == 0 && _tail_closed) {
                    _head_closed = true;
                }
                break;
            case NEED_WRAP:
                // keep looping
                continue;
            case NEED_TASK:
                runDelegatedTasks(result);
                continue;
            case FINISHED:
                updateCipherAndProtocolName(result);
                // intentionally fall through
            case NOT_HANDSHAKING:
                if (pending > 0 && status == Status.OK) {
                    continue;
                } else {
                    break;
                }
            }

            break;
        }
    }

    private boolean hasSpaceForSslPacket(ByteBuffer byteBuffer)
    {
        return byteBuffer.remaining() >= _sslEngine.getPacketBufferSize();
    }

    /** @return the cipher name, which is null until the SSL handshaking is completed */
    @Override
    public String getCipherName()
    {
        return _cipherName;
    }

    /** @return the protocol name, which is null until the SSL handshaking is completed */
    @Override
    public String getProtocolName()
    {
        return _protocolName;
    }

    private void updateCipherAndProtocolName(SSLEngineResult result)
    {
        if (result.getHandshakeStatus() == HandshakeStatus.FINISHED)
        {
            _cipherName = _sslEngine.getCipherSuite();
            _protocolName = _sslEngine.getProtocol();
        }
    }

    private void runDelegatedTasks(SSLEngineResult result)
    {
        if (result.getHandshakeStatus() == HandshakeStatus.NEED_TASK)
        {
            Runnable runnable;
            while ((runnable = _sslEngine.getDelegatedTask()) != null)
            {
                runnable.run();
            }

            HandshakeStatus hsStatus = _sslEngine.getHandshakeStatus();
            if (hsStatus == HandshakeStatus.NEED_TASK)
            {
                throw new RuntimeException("handshake shouldn't need additional tasks");
            }
        }
    }

    private void logEngineClientModeAndResult(SSLEngineResult result, String direction)
    {
        if(_logger.isLoggable(Level.FINEST))
        {
            _logger.log(Level.FINEST, "useClientMode = " + _sslEngine.getUseClientMode() + " direction = " + direction
                        + " " + resultToString(result));
        }
    }

    private String resultToString(SSLEngineResult result)
    {
        return new StringBuilder("[SSLEngineResult status = ").append(result.getStatus())
                .append(" handshakeStatus = ").append(result.getHandshakeStatus())
                .append(" bytesConsumed = ").append(result.bytesConsumed())
                .append(" bytesProduced = ").append(result.bytesProduced())
                .append("]").toString();
    }

    @Override
    public int capacity()
    {
        if (_tail_closed) return Transport.END_OF_STREAM;
        return _inputBuffer.remaining();
    }

    @Override
    public ByteBuffer tail()
    {
        if (_tail_closed) throw new TransportException("tail closed");
        return _inputBuffer;
    }

    @Override
    public void process() throws TransportException
    {
        if (_tail_closed) throw new TransportException("tail closed");

        _inputBuffer.flip();

        try {
            unwrapInput();
        } catch (SSLException e) {
            _logger.log(Level.WARNING, e.getMessage());
            _inputBuffer.position(_inputBuffer.limit());
            _tail_closed = true;
        } finally {
            _inputBuffer.compact();
        }
    }

    @Override
    public void close_tail()
    {
        try {
            _underlyingInput.close_tail();
        } finally {
            _tail_closed = true;
        }
    }

    @Override
    public int pending()
    {
        try {
            wrapOutput();
        } catch (SSLException e) {
            _logger.log(Level.WARNING, e.getMessage());
            _head_closed = true;
        }

        _head.limit(_outputBuffer.position());

        if (_head_closed && _outputBuffer.position() == 0) {
            return Transport.END_OF_STREAM;
        }

        return _outputBuffer.position();
    }

    @Override
    public ByteBuffer head()
    {
        pending();
        return _head;
    }

    @Override
    public void pop(int bytes)
    {
        _outputBuffer.flip();
        _outputBuffer.position(bytes);
        _outputBuffer.compact();
        _head.position(0);
        _head.limit(_outputBuffer.position());
    }

    @Override
    public void close_head()
    {
        _underlyingOutput.close_head();
        int p = pending();
        if (p > 0) {
            pop(p);
        }
    }


    @Override
    public String toString()
    {
        StringBuilder builder = new StringBuilder();
        builder.append("SimpleSslTransportWrapper [sslEngine=").append(_sslEngine)
            .append(", inputBuffer=").append(_inputBuffer)
            .append(", outputBuffer=").append(_outputBuffer)
            .append(", decodedInputBuffer=").append(_decodedInputBuffer)
            .append(", cipherName=").append(_cipherName)
            .append(", protocolName=").append(_protocolName)
            .append("]");
        return builder.toString();
    }
}
