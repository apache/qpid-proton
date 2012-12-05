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
import java.util.logging.Level;
import java.util.logging.Logger;

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
 * TODO move SSL and possible byte management classes into separate package
 */
public class SimpleSslTransportWrapper implements SslTransportWrapper
{
    private static final Logger _logger = Logger.getLogger(SimpleSslTransportWrapper.class.getName());

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

    /** Used by {@link #output(byte[], int, int)}. Acts as a buffer for the output from underlyingOutput */
    private ByteHolder _clearOutputHolder;

    /** Used by {@link #output(byte[], int, int)}. Caches the output from SSLEngine.wrap */
    private ByteHolder _encodedOutputHolder;

    /** Used by {@link #input(byte[], int, int)}. A buffer for the decoded bytes that will be passed to _underlyingInput */
    private ByteHolder _decodedInputHolder;

    /** used by {@link #input(byte[], int, int)} to cache the left over bytes not yet decoded due to buffer underflow. */
    private final BytePipeline _encodedLeftoversPipeline = new BytePipeline();

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
        createByteHolders();
    }

    private void createByteHolders()
    {
        int appBufferMax = _sslEngine.getApplicationBufferSize();
        int packetBufferMax = _sslEngine.getPacketBufferSize();

        _clearOutputHolder = new ByteHolder(appBufferMax + APPLICATION_BUFFER_EXTRA);
        _decodedInputHolder = new ByteHolder(appBufferMax + APPLICATION_BUFFER_EXTRA);
        _decodedInputHolder.prepareToRead();

        _encodedOutputHolder = new ByteHolder(packetBufferMax);
        _encodedOutputHolder.prepareToRead();
    }

    @Override
    public int input(byte[] encodedBytes, int offset, int size)
    {
        final int initialSizeOfEncodedLeftovers =  _encodedLeftoversPipeline.getSize();
        final ByteBuffer oldPlusNewEncodedByteBuffer = _encodedLeftoversPipeline.appendAndClear(encodedBytes, offset, size);

        if(!_decodedInputHolder.readInto(_underlyingInput))
        {
            // underlying input couldn't accept all existing leftovers so we return without trying to decode any new data
            return 0;
        }

        try
        {
            boolean keepLooping = true;
            int totalBytesDecoded = 0;

            do
            {
                SSLEngineResult result = _sslEngine.unwrap(oldPlusNewEncodedByteBuffer, _decodedInputHolder.prepareToWrite());
                _decodedInputHolder.prepareToRead();

                runDelegatedTasks(result);
                updateCipherAndProtocolName(result);

                if(_logger.isLoggable(Level.FINEST))
                {
                    _logger.log(Level.FINEST, _sslParams.getMode() + " input " + resultToString(result));
                }

                Status sslResultStatus = result.getStatus();
                HandshakeStatus handshakeStatus = result.getHandshakeStatus();

                if(sslResultStatus == SSLEngineResult.Status.OK)
                {
                    totalBytesDecoded += result.bytesConsumed();
                }
                else if(sslResultStatus == SSLEngineResult.Status.BUFFER_UNDERFLOW)
                {
                    // Not an error. We store the not-yet-decoded bytes which will hopefully be augmented by some more
                    // in a subsequent invocation of this method, allowing decoding to be done.
                    _encodedLeftoversPipeline.set(oldPlusNewEncodedByteBuffer, totalBytesDecoded);
                }
                else
                {
                    throw new IllegalStateException("Unexpected SSL Engine state " + sslResultStatus);
                }

                boolean allAcceptedByUnderlyingInput = true;
                if(result.bytesProduced() > 0)
                {
                    if(handshakeStatus != HandshakeStatus.NOT_HANDSHAKING)
                    {
                        _logger.warning("WARN unexpectedly produced bytes for the underlying input when handshaking");
                    }

                    allAcceptedByUnderlyingInput = _decodedInputHolder.readInto(_underlyingInput);
                }

                keepLooping = handshakeStatus == HandshakeStatus.NOT_HANDSHAKING
                            && sslResultStatus != Status.BUFFER_UNDERFLOW
                            && allAcceptedByUnderlyingInput;
            }
            while(keepLooping);

            int newEncodedLeftovers = _encodedLeftoversPipeline.getSize();
            int sizeToReportConsumed = totalBytesDecoded + newEncodedLeftovers - initialSizeOfEncodedLeftovers;
            return sizeToReportConsumed;
        }
        catch(SSLException e)
        {
            throw new TransportException(e);
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

    /**
     * Write encoded output to the supplied destination.
     *
     * The following conditions hold before and after this method call:
     * {@link #_encodedOutputHolder} is readable.
     * {@link #_clearOutputHolder} is writeable.
     */
    @Override
    public int output(byte[] destination, int offset, final int size)
    {
        try
        {
            int totalBytesWrittenToDestination = _encodedOutputHolder.readInto(destination, offset, size);
            if(totalBytesWrittenToDestination == size)
            {
                return size;
            }

            boolean keepLooping = true;
            do
            {
                if (_clearOutputHolder.hasSpace())
                {
                    int numberOfBytesInClearOutputHolder = _clearOutputHolder.writeOutputFrom(_underlyingOutput);
                    if (numberOfBytesInClearOutputHolder == 0)
                    {
                        break; // no output to wrap
                    }
                }

                int availableDestinationSize = size - totalBytesWrittenToDestination;
                // SSLEngine will wrap directly into the provided destination byte[] if there is space, or into _encodedOutputHolder otherwise.
                boolean wrappingIntoEncodedOutputHolder = availableDestinationSize < _sslEngine.getPacketBufferSize();
                final ByteBuffer sslWrapDst;
                if (wrappingIntoEncodedOutputHolder)
                {
                    sslWrapDst = _encodedOutputHolder.prepareToWrite();
                }
                else
                {
                    sslWrapDst = ByteBuffer.wrap(destination, offset+totalBytesWrittenToDestination, availableDestinationSize);
                }

                SSLEngineResult result = _sslEngine.wrap(_clearOutputHolder.prepareToRead(), sslWrapDst);
                _clearOutputHolder.prepareToWrite();

                Status sslResultStatus = result.getStatus();
                if(sslResultStatus == SSLEngineResult.Status.BUFFER_OVERFLOW)
                {
                    throw new IllegalStateException("Insufficient space to perform wrap into encoded output buffer that has " + sslWrapDst.remaining() + " remaining bytes. wrappingIntoEncodedOutputHolder=" + wrappingIntoEncodedOutputHolder);
                }
                else if(sslResultStatus != SSLEngineResult.Status.OK)
                {
                    throw new RuntimeException("Unexpected SSLEngineResult status " + sslResultStatus);
                }

                runDelegatedTasks(result);
                updateCipherAndProtocolName(result);

                final int numberOfNewlyEncodedBytesWritten = Math.min(availableDestinationSize, result.bytesProduced());
                if(wrappingIntoEncodedOutputHolder)
                {
                    _encodedOutputHolder.prepareToRead();
                    _encodedOutputHolder.readInto(destination, offset+totalBytesWrittenToDestination, numberOfNewlyEncodedBytesWritten);
                }

                totalBytesWrittenToDestination += numberOfNewlyEncodedBytesWritten;

                keepLooping = result.getHandshakeStatus() == HandshakeStatus.NOT_HANDSHAKING
                        && totalBytesWrittenToDestination < size;
            }
            while(keepLooping);

            return totalBytesWrittenToDestination;
        }
        catch(SSLException e)
        {
            throw new TransportException("Mode " + _sslParams.getMode(), e);
        }
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

    public static void main(String[] args)
    {
        _logger.info("PHDEBUG in main");
        _logger.info("PHDEBUG in main2");
    }
}
