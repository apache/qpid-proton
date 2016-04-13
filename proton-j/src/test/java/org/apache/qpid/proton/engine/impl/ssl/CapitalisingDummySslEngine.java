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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.nio.ByteBuffer;

import javax.net.ssl.SSLEngineResult;
import javax.net.ssl.SSLEngineResult.HandshakeStatus;
import javax.net.ssl.SSLEngineResult.Status;
import javax.net.ssl.SSLException;



/**
 * A simpler implementation of an SSLEngine that has predictable human-readable output, and that allows us to
 * easily trigger {@link Status#BUFFER_OVERFLOW} and {@link Status#BUFFER_UNDERFLOW}.
 *
 * Using a true SSLEngine for this would be impractical.
 */
public class CapitalisingDummySslEngine implements ProtonSslEngine
{
    static final int SHORT_ENCODED_CHUNK_SIZE = 2;
    static final int MAX_ENCODED_CHUNK_SIZE = 5;
    private static final char ENCODED_TEXT_BEGIN = '<';
    private static final char ENCODED_TEXT_END = '>';
    private static final char ENCODED_TEXT_INNER_CHAR = '-';

    private static final int CLEAR_CHUNK_SIZE = 2;
    private static final char CLEARTEXT_PADDING = '_';
    private SSLException _nextException;
    private int _applicationBufferSize = CLEAR_CHUNK_SIZE;
    private int _packetBufferSize = MAX_ENCODED_CHUNK_SIZE;
    private int _unwrapCount;

    /**
     * Converts a_ to <-A->.  z_ is special and encodes as <> (to give us packets of different lengths).
     * If dst is not sufficiently large ({@value #SHORT_ENCODED_CHUNK_SIZE} in our encoding), we return
     * {@link Status#BUFFER_OVERFLOW}, and the src and dst ByteBuffers are unchanged.
     */
    @Override
    public SSLEngineResult wrap(ByteBuffer src, ByteBuffer dst)
            throws SSLException
    {
        int consumed = 0;
        int produced = 0;
        final Status resultStatus;

        if (src.remaining() >= CLEAR_CHUNK_SIZE)
        {
            src.mark();

            char uncapitalisedChar = (char) src.get();
            char underscore = (char) src.get();

            validateClear(uncapitalisedChar, underscore);

            boolean useShortEncoding = uncapitalisedChar == 'z';
            int encodingLength = useShortEncoding ? SHORT_ENCODED_CHUNK_SIZE : MAX_ENCODED_CHUNK_SIZE;
            boolean overflow = dst.remaining() < encodingLength;

            if (overflow)
            {
                src.reset();
                resultStatus = Status.BUFFER_OVERFLOW;
            }
            else
            {
                consumed = CLEAR_CHUNK_SIZE;

                char capitalisedChar = Character.toUpperCase(uncapitalisedChar);

                dst.put((byte)ENCODED_TEXT_BEGIN);
                if (!useShortEncoding)
                {
                    dst.put((byte)ENCODED_TEXT_INNER_CHAR);
                    dst.put((byte)capitalisedChar);
                    dst.put((byte)ENCODED_TEXT_INNER_CHAR);
                }
                dst.put((byte)ENCODED_TEXT_END);
                produced = encodingLength;

                resultStatus = Status.OK;
            }
        }
        else
        {
            resultStatus = Status.OK;
        }

        return new SSLEngineResult(resultStatus, HandshakeStatus.NOT_HANDSHAKING, consumed, produced);
    }

    /**
     * Converts <-A-><-B-><-C-> to a_. <> is special and decodes as z_
     * Input such as "<A" will causes a {@link Status#BUFFER_UNDERFLOW} result status.
     */
    @Override
    public SSLEngineResult unwrap(ByteBuffer src, ByteBuffer dst)
            throws SSLException
    {
        _unwrapCount++;

        if(_nextException != null)
        {
            throw _nextException;
        }

        Status resultStatus;
        final int consumed;
        final int produced;

        if (src.remaining() >= SHORT_ENCODED_CHUNK_SIZE)
        {
            src.mark();

            char begin = (char)src.get();
            char nextChar = (char)src.get(); // Could be - or >
            final int readSoFar = 2;
            final char capitalisedChar;

            if (nextChar != ENCODED_TEXT_END)
            {
                int remainingBytesForMaxLengthPacket = MAX_ENCODED_CHUNK_SIZE - readSoFar;
                if (src.remaining() < remainingBytesForMaxLengthPacket )
                {
                    src.reset();
                    resultStatus = Status.BUFFER_UNDERFLOW;
                    return new SSLEngineResult(resultStatus, HandshakeStatus.NOT_HANDSHAKING, 0, 0);
                }
                else
                {
                    char beginInner = nextChar;
                    capitalisedChar = (char)src.get();
                    char endInner = (char)src.get();
                    char end = (char)src.get();
                    consumed = MAX_ENCODED_CHUNK_SIZE;
                    validateEncoded(begin, beginInner, capitalisedChar, endInner, end);
                }
            }
            else
            {
                assertEquals("Unexpected begin", Character.toString(ENCODED_TEXT_BEGIN), Character.toString(begin));
                capitalisedChar = 'Z';
                consumed = SHORT_ENCODED_CHUNK_SIZE;;
            }

            char lowerCaseChar = Character.toLowerCase(capitalisedChar);
            dst.put((byte)lowerCaseChar);
            dst.put((byte)CLEARTEXT_PADDING);
            produced = CLEAR_CHUNK_SIZE;

            resultStatus = Status.OK;
        }
        else
        {
            resultStatus = Status.BUFFER_UNDERFLOW;
            consumed = 0;
            produced = 0;
        }

        return new SSLEngineResult(resultStatus, HandshakeStatus.NOT_HANDSHAKING, consumed, produced);
    }

    @Override
    public int getEffectiveApplicationBufferSize()
    {
        return getApplicationBufferSize();
    }

    private int getApplicationBufferSize()
    {
        return _applicationBufferSize;
    }

    @Override
    public int getPacketBufferSize()
    {
        return _packetBufferSize;
    }

    public void setApplicationBufferSize(int value)
    {
        _applicationBufferSize = value;
    }

    public void setPacketBufferSize(int value)
    {
        _packetBufferSize = value;
    }

    @Override
    public String getProtocol()
    {
        throw new UnsupportedOperationException();
    }

    @Override
    public HandshakeStatus getHandshakeStatus()
    {
        throw new UnsupportedOperationException();
    }

    @Override
    public Runnable getDelegatedTask()
    {
        throw new UnsupportedOperationException();
    }

    @Override
    public String getCipherSuite()
    {
        throw new UnsupportedOperationException();
    }


    private void validateEncoded(char begin, char beginInner, char capitalisedChar, char endInner, char end)
    {
        assertEquals("Unexpected begin", Character.toString(ENCODED_TEXT_BEGIN), Character.toString(begin));
        assertEquals("Unexpected begin inner", Character.toString(ENCODED_TEXT_INNER_CHAR), Character.toString(beginInner));
        assertEquals("Unexpected end inner", Character.toString(ENCODED_TEXT_INNER_CHAR), Character.toString(endInner));
        assertEquals("Unexpected end", Character.toString(ENCODED_TEXT_END), Character.toString(end));
        assertTrue("Encoded character " + capitalisedChar + " must be capital", Character.isUpperCase(capitalisedChar));
    }

    private void validateClear(char uncapitalisedChar, char underscore)
    {
        assertTrue("Clear text character " + uncapitalisedChar + " must be lowercase", Character.isLowerCase(uncapitalisedChar));
        assertEquals("Unexpected clear text pad", Character.toString(CLEARTEXT_PADDING), Character.toString(underscore));
    }

    @Override
    public boolean getUseClientMode()
    {
        return true;
    }

    public void rejectNextEncodedPacket(SSLException nextException)
    {
        _nextException = nextException;
    }

    int getUnwrapCount() {
        return _unwrapCount;
    }
}
