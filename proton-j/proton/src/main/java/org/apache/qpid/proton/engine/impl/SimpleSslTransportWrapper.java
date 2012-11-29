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

import java.nio.ByteBuffer;

import javax.net.ssl.SSLEngine;
import javax.net.ssl.SSLEngineResult;
import javax.net.ssl.SSLEngineResult.HandshakeStatus;
import javax.net.ssl.SSLEngineResult.Status;
import javax.net.ssl.SSLException;
import javax.net.ssl.SSLSession;

import org.apache.qpid.proton.engine.Ssl;
import org.apache.qpid.proton.engine.TransportException;
import org.apache.qpid.proton.engine.TransportInput;
import org.apache.qpid.proton.engine.TransportOutput;

/**
 * TODO close the SSLEngine when told to, and modify {@link #input(byte[], int, int)} and {@link #output(byte[], int, int)}
 * to respond appropriately thereafter.
 */
public class SimpleSslTransportWrapper implements SslTransportWrapper
{
    /**
     * Our testing has shown that application buffers need to be a bit larger
     * than that provided by {@link SSLSession#getApplicationBufferSize()} otherwise
     * {@link Status#BUFFER_OVERFLOW} will result on {@link SSLEngine#unwrap()}.
     * Sun's own example uses 50, so we use the same.
     */
    private static final int APPLICATION_BUFFER_EXTRA = 50;

    private final TransportInput _underlyingInput;
    private final TransportOutput _underlyingOutput;
    private final Ssl _sslParams;

    private SslEngineFacade _sslEngine;

    private byte[] _clearOutputBuf;
    private int _clearOutputBufOffset;
    /** how many bytes of leftover clear output we've got */
    private int _clearOutputBufSize;
    private ByteBuffer _encodedOutputBuf;

    private byte[] _encodedLeftovers = new byte[0];

    private byte[] _clearLeftoverInput;
    private int _clearLeftovertInputOffset;
    private int _clearLeftoverInputSize;


    /** could change during the lifetime of the ssl connection owing to renegotiation. */
    private String _cipherName;

    /** could change during the lifetime of the ssl connection owing to renegotiation. */
    private String _protocolName;

    private final SslEngineFacadeFactory _sslEngineFacadeFactory;

    public SimpleSslTransportWrapper(Ssl sslConfiguration, TransportInput underlyingInput, TransportOutput underlyingOutput)
    {
        this(sslConfiguration, underlyingInput, underlyingOutput, new SslEngineFacadeFactory());
    }

    SimpleSslTransportWrapper(Ssl sslConfiguration,
            TransportInput underlyingInput, TransportOutput underlyingOutput,
            SslEngineFacadeFactory sslEngineFacadeFactory)
    {
        _sslParams = sslConfiguration;
        _underlyingInput = underlyingInput;
        _underlyingOutput = underlyingOutput;
        _sslEngineFacadeFactory = sslEngineFacadeFactory;
        _sslEngine = _sslEngineFacadeFactory.createSslEngineFacade(sslConfiguration);
        createBuffers(_sslEngine);
    }

    private void createBuffers(SslEngineFacade sslEngine)
    {
        int appBufferMax = sslEngine.getApplicationBufferSize();
        int packetBufferMax = sslEngine.getPacketBufferSize();

        _clearOutputBuf = new byte[appBufferMax + APPLICATION_BUFFER_EXTRA];
        _clearLeftoverInput = new byte[appBufferMax + APPLICATION_BUFFER_EXTRA];

        _encodedOutputBuf = ByteBuffer.allocate(packetBufferMax);
        _encodedOutputBuf.limit(0);
    }

    @Override
    public int input(byte[] encodedBytes, int offset, int size)
    {
        final int initialSizeOfEncodedLeftovers = getSizeOfEncodedLeftovers();
        final byte[] oldPlusNewEncodedBytes = buildLeftoversPlusNewEncodedBytes(encodedBytes, offset, size);
        final ByteBuffer oldPlusNewEncodedByteBuffer = ByteBuffer.wrap(oldPlusNewEncodedBytes);
        clearEncodedLeftovers();

        if (_clearLeftoverInputSize > 0)
        {
            int numberAccepted = _underlyingInput.input(_clearLeftoverInput, _clearLeftovertInputOffset, _clearLeftoverInputSize);
            if (numberAccepted < _clearLeftoverInputSize)
            {
                // underlying input couldn't accept all existing leftovers so we return without trying to decode any new data
                _clearLeftoverInputSize -= numberAccepted;
                _clearLeftovertInputOffset += numberAccepted;
                return 0;
            }
            else
            {
                _clearLeftoverInputSize = 0;
                _clearLeftovertInputOffset = 0;
            }
        }

        try
        {
            boolean keepLooping = true;

            byte[] clearBytes = new byte[_sslEngine.getApplicationBufferSize() + APPLICATION_BUFFER_EXTRA];
            ByteBuffer clearByteBuffer = ByteBuffer.wrap(clearBytes);
            int totalBytesConsumed = 0;

            do
            {
                SSLEngineResult result = _sslEngine.unwrap(oldPlusNewEncodedByteBuffer, clearByteBuffer);

                runDelegatedTasks(result);
                updateCipherAndProtocolName(result);

                System.out.println(_sslParams.getMode() + " input " + sslEngineResultToString(result));

                if(result.getStatus() == SSLEngineResult.Status.BUFFER_UNDERFLOW)
                {
                    cacheEncodedLeftovers(oldPlusNewEncodedBytes, totalBytesConsumed);
                }
                else if(result.getStatus() == SSLEngineResult.Status.OK)
                {
                    totalBytesConsumed += result.bytesConsumed();
                }
                else
                {
                    throw new IllegalStateException("Unexpected SSL Engine state " + result.getStatus());
                }

                HandshakeStatus handshakeStatus = result.getHandshakeStatus();
                int sizeOfClearLeftovers = 0;
                if(result.bytesProduced() > 0)
                {
                    if(handshakeStatus != HandshakeStatus.NOT_HANDSHAKING)
                    {
                        System.out.println("WARN unexpectedly produced bytes for the underlying input when handshaking");
                    }

                    int numberAccepted = _underlyingInput.input(clearBytes, 0, result.bytesProduced());
                    sizeOfClearLeftovers = result.bytesProduced() - numberAccepted;
                    if (sizeOfClearLeftovers > 0)
                    {
                        System.arraycopy(clearBytes, numberAccepted, _clearLeftoverInput, 0, sizeOfClearLeftovers);
                        _clearLeftovertInputOffset = 0;
                        _clearLeftoverInputSize = sizeOfClearLeftovers;
                    }

                    clearByteBuffer.clear();
                }

                keepLooping = handshakeStatus == HandshakeStatus.NOT_HANDSHAKING
                            && result.getStatus() != Status.BUFFER_UNDERFLOW
                            && sizeOfClearLeftovers == 0;
            }
            while(keepLooping);

            int newEncodedLeftovers = getSizeOfEncodedLeftovers();
            int sizeToReportConsumed = totalBytesConsumed - initialSizeOfEncodedLeftovers + newEncodedLeftovers;
            return sizeToReportConsumed;
        }
        catch(SSLException e)
        {
            throw new TransportException(e);
        }
    }

    private String sslEngineResultToString(SSLEngineResult result)
    {
        return new StringBuilder("[SSLEngineResult status = ").append(result.getStatus())
                .append(" handshakeStatus = ").append(result.getHandshakeStatus())
                .append(" bytesConsumed = ").append(result.bytesConsumed())
                .append(" bytesProduced = ").append(result.bytesProduced())
                .append("]").toString();
    }

    private void clearEncodedLeftovers()
    {
        _encodedLeftovers = new byte[0];
    }

    private int getSizeOfEncodedLeftovers()
    {
        return _encodedLeftovers.length;
    }

    private byte[] buildLeftoversPlusNewEncodedBytes(byte[] encodedBytes, int offset, int size)
    {
        // TODO revisit code below and consider avoiding the multiple byte array allocations.
        int sizeOfResult = _encodedLeftovers.length + size;
        byte[] resultBytes = new byte[sizeOfResult];

        System.arraycopy(_encodedLeftovers, 0, resultBytes, 0, _encodedLeftovers.length);

        int destPos = _encodedLeftovers.length;
        System.arraycopy(encodedBytes, offset, resultBytes, destPos, size);

        return resultBytes;
    }


    private void cacheEncodedLeftovers(byte[] encodedBytes, int numberAlreadyConsumed)
    {
        int leftoverSize = encodedBytes.length - numberAlreadyConsumed;
        _encodedLeftovers = new byte[leftoverSize];
        System.arraycopy(encodedBytes, numberAlreadyConsumed, _encodedLeftovers, 0, leftoverSize);
    }

    /*
     * The state of _encodedOutputBuf is interesting.
     *
     * Pre-condition: it may contain stuff. It is ready for this stuff to be read into encodedBytesDestination
     * Post-condition: it may contain stuff. It is ready for this stuff to be read into encodedBytesDestination
     */
    @Override
    public int output(byte[] encodedBytesDestination, int offset, final int size)
    {
        try
        {
            int numberOfEncodedLeftovers = 0;
            if(_encodedOutputBuf.hasRemaining())
            {
                // We already had some data ready encoded from the last call
                numberOfEncodedLeftovers = _encodedOutputBuf.remaining();
                if(numberOfEncodedLeftovers >= size)
                {
                    // our leftovers are sufficient to satisfy the "size" requested so fill the destination and return
                    _encodedOutputBuf.get(encodedBytesDestination,offset,size);
                    return size;
                }
                else
                {
                    // copy in the bytes we have leaving the byte buffer in a reading state
                    _encodedOutputBuf.get(encodedBytesDestination,offset,numberOfEncodedLeftovers);
                }
            }

            int totalNumberOfBytesWrittenToEncodedBytesDestination = numberOfEncodedLeftovers;
            boolean keepLooping = true;
            do
            {
                if(_clearOutputBufSize < _clearOutputBuf.length)
                {
                    // Get some more clear bytes from the underlying output
                    int outSize = _underlyingOutput.output(_clearOutputBuf, _clearOutputBufOffset, _clearOutputBuf.length - _clearOutputBufOffset);
                    if(outSize > 0)
                    {
                        _clearOutputBufSize += outSize;
                    }
                    if (_clearOutputBufSize == 0)
                    {
                        // No output to wrap.
                        break;
                    }

                }

                ByteBuffer src = ByteBuffer.wrap(_clearOutputBuf,_clearOutputBufOffset,_clearOutputBufSize);

                final ByteBuffer dst;
                int availableDestinationSize = size - totalNumberOfBytesWrittenToEncodedBytesDestination;
                boolean wrappingIntoEncodedOutputBuf = availableDestinationSize < _sslEngine.getPacketBufferSize();
                if (wrappingIntoEncodedOutputBuf)
                {
                    dst = _encodedOutputBuf;
                    _encodedOutputBuf.clear(); // get ready to write to it
                }
                else
                {
                    dst = ByteBuffer.wrap(encodedBytesDestination, offset+totalNumberOfBytesWrittenToEncodedBytesDestination, availableDestinationSize);
                }

                SSLEngineResult result = _sslEngine.wrap(src, dst);
                _clearOutputBufOffset += _clearOutputBufSize - src.remaining();

                if(result.getStatus() == SSLEngineResult.Status.BUFFER_OVERFLOW)
                {
                    // Should not happen as _encodedOutputBuf is sized by getPacketBufferSize or
                    // encodedBytesDestination has sufficient space for one packet
                    throw new IllegalStateException("Insufficient space to perform wrap into encoded output buffer that has " + dst.remaining() + " remaining bytes");
                }

                System.out.println(_sslParams.getMode() + " output " + sslEngineResultToString(result) + " dst buf len " + _clearOutputBufSize);
                runDelegatedTasks(result);
                updateCipherAndProtocolName(result);

                final int numberOfNewlyEncodedBytesWritten = Math.min(availableDestinationSize, result.bytesProduced());
                if(wrappingIntoEncodedOutputBuf)
                {
                    _encodedOutputBuf.flip(); // get ready to read from it

                    // Now take as many bytes as we can into encodedBytesDestination (up to numberOfNewlyEncodedBytesWritten)
                    // leaving the remainder to be read from _encodedOutputBuf on the next invocation
                    _encodedOutputBuf.get(encodedBytesDestination, offset+totalNumberOfBytesWrittenToEncodedBytesDestination, numberOfNewlyEncodedBytesWritten);
                }

                if(result.getStatus() != SSLEngineResult.Status.OK)
                {
                    throw new RuntimeException("Output Uh Oh! " + result.getStatus());
                }


                if(_clearOutputBufOffset == _clearOutputBufSize)
                {
                    _clearOutputBufOffset = _clearOutputBufSize = 0;
                }

                totalNumberOfBytesWrittenToEncodedBytesDestination += numberOfNewlyEncodedBytesWritten;

                HandshakeStatus handshakeStatus = result.getHandshakeStatus();

                keepLooping = handshakeStatus == HandshakeStatus.NOT_HANDSHAKING
                        && totalNumberOfBytesWrittenToEncodedBytesDestination < size;
            }
            while(keepLooping);
            return totalNumberOfBytesWrittenToEncodedBytesDestination;

        }
        catch(SSLException e)
        {
            throw new TransportException("Mode " + _sslParams.getMode(), e);
        }

        // first write remaining encoded output into passed buffers (bytes)
        // if anything left in plain output, run through ssl engine into encoded output, then write into passed buffers (bytes)
        // if still space in bytes, then call output on underlying transport to fill plain output, and repeat step above

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

}
