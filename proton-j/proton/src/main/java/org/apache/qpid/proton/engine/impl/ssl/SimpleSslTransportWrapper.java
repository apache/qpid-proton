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

import org.apache.qpid.proton.engine.TransportException;
import org.apache.qpid.proton.engine.TransportResult;
import org.apache.qpid.proton.engine.TransportResultFactory;
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

    private final ByteBuffer _inputBuffer;

    private final ByteBuffer _outputBuffer;

    /**
     * A buffer for the decoded bytes that will be passed to _underlyingInput.
     * This extra layer of buffering is necessary in case the underlying input's buffer
     * is too small for SSLEngine to ever unwrap into.
     */
    private final ByteBuffer _decodedInputBuffer;

    private ByteBuffer _readOnlyOutputBufferView;

    /** could change during the lifetime of the ssl connection owing to renegotiation. */
    private String _cipherName;

    /** could change during the lifetime of the ssl connection owing to renegotiation. */
    private String _protocolName;


    /**
     * We store the last input result so that we know not to process any more input
     * if it failed.
     */
    private TransportResult _lastInputResult = TransportResultFactory.ok();


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

        _decodedInputBuffer = newReadableBuffer(effectiveAppBufferMax);

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
    private TransportResult unwrapInput()
    {
        try
        {
            TransportResult underlyingResult = TransportResultFactory.ok();
            boolean keepLooping = true;
            do
            {
                _decodedInputBuffer.compact();
                SSLEngineResult result = _sslEngine.unwrap(_inputBuffer, _decodedInputBuffer);
                _decodedInputBuffer.flip();

                runDelegatedTasks(result);
                updateCipherAndProtocolName(result);

                logEngineClientModeAndResult(result, "input");

                Status sslResultStatus = result.getStatus();
                HandshakeStatus handshakeStatus = result.getHandshakeStatus();

                if(sslResultStatus == SSLEngineResult.Status.OK)
                {
                    // continue
                }
                else if(sslResultStatus == SSLEngineResult.Status.BUFFER_UNDERFLOW)
                {
                    // Not an error. The not-yet-decoded bytes remain in _inputBuffer and will hopefully be augmented by some more
                    // in a subsequent invocation of this method, allowing decoding to be done.
                }
                else
                {
                    throw new IllegalStateException("Unexpected SSL Engine state " + sslResultStatus);
                }

                if(result.bytesProduced() > 0)
                {
                    if(handshakeStatus != HandshakeStatus.NOT_HANDSHAKING)
                    {
                        _logger.warning("WARN unexpectedly produced bytes for the underlying input when handshaking");
                    }

                    underlyingResult = pourAll(_decodedInputBuffer, _underlyingInput);
                }

                keepLooping = handshakeStatus == HandshakeStatus.NOT_HANDSHAKING
                            && sslResultStatus != Status.BUFFER_UNDERFLOW
                            && underlyingResult.isOk();
            }
            while(keepLooping);

            return underlyingResult;
        }
        catch(SSLException e)
        {
            return TransportResultFactory.error(new TransportException(
                    "Problem during input. useClientMode: " + _sslEngine.getUseClientMode(), e));
        }
    }

    /**
     * Wrap the underlying transport's output, passing it to the output buffer.
     *
     * {@link #_outputBuffer} is assumed to be writeable on entry and is guaranteed to
     * be still writeable on exit.
     */
    private void wrapOutput()
    {
        boolean keepWrapping = hasSpaceForSslPacket(_outputBuffer);
        while(keepWrapping)
        {
            ByteBuffer clearOutputBuffer = _underlyingOutput.getOutputBuffer();
            try
            {
                if(clearOutputBuffer.hasRemaining())
                {
                    SSLEngineResult result = _sslEngine.wrap(clearOutputBuffer, _outputBuffer);

                    logEngineClientModeAndResult(result, "output");

                    Status sslResultStatus = result.getStatus();
                    if(sslResultStatus == SSLEngineResult.Status.BUFFER_OVERFLOW)
                    {
                        throw new IllegalStateException("Insufficient space to perform wrap into encoded output buffer. Buffer: " + _outputBuffer);
                    }
                    else if(sslResultStatus != SSLEngineResult.Status.OK)
                    {
                        throw new RuntimeException("Unexpected SSLEngineResult status " + sslResultStatus);
                    }

                    runDelegatedTasks(result);
                    updateCipherAndProtocolName(result);

                    keepWrapping = result.getHandshakeStatus() == HandshakeStatus.NOT_HANDSHAKING
                            && hasSpaceForSslPacket(_outputBuffer);
                }
                else
                {
                    // no output to wrap
                    keepWrapping = false;
                }
            }
            catch(SSLException e)
            {
                throw new TransportException("Problem during output. useClientMode: " + _sslEngine.getUseClientMode(), e);
            }
            finally
            {
                _underlyingOutput.outputConsumed();
            }
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
    public ByteBuffer getInputBuffer()
    {
        _lastInputResult.checkIsOk();
        return _inputBuffer;
    }

    @Override
    public TransportResult processInput()
    {
        _inputBuffer.flip();

        try
        {
            _lastInputResult = unwrapInput();
            return _lastInputResult;
        }
        finally
        {
            _inputBuffer.compact();
        }
    }

    /*
     * Pre-condition of {@link #_outputBuffer}: it's writeable
     * Post-condition of {@link #_outputBuffer}: it's readable
     */
    @Override
    public ByteBuffer getOutputBuffer()
    {
        wrapOutput();
        _outputBuffer.flip();

        _readOnlyOutputBufferView = _outputBuffer.asReadOnlyBuffer();
        return _readOnlyOutputBufferView;
    }

    @Override
    public void outputConsumed()
    {
        if(_readOnlyOutputBufferView == null)
        {
            throw new IllegalStateException("Illegal invocation with previously calling getOutputBuffer");
        }

        _outputBuffer.position(_readOnlyOutputBufferView.position());
        _readOnlyOutputBufferView = null;
        _outputBuffer.compact();
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
