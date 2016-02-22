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

import static org.apache.qpid.proton.engine.impl.ByteBufferUtils.pour;
import static org.apache.qpid.proton.engine.impl.TransportTestHelper.assertByteBufferContentEquals;
import static org.apache.qpid.proton.engine.impl.TransportTestHelper.pourBufferToString;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.nio.ByteBuffer;

import javax.net.ssl.SSLException;

import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.engine.TransportException;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;

/**
 * TODO unit test handshaking
 * TODO unit test closing
 * TODO unit test graceful handling of SSLEngine.wrap throwing an SSLException
 */
public class SimpleSslTransportWrapperTest
{
    private RememberingTransportInput _underlyingInput = new RememberingTransportInput();
    private CannedTransportOutput _underlyingOutput = new CannedTransportOutput();

    private SimpleSslTransportWrapper _sslWrapper;

    private CapitalisingDummySslEngine _dummySslEngine = new CapitalisingDummySslEngine();

    @Rule
    public ExpectedException _expectedException = ExpectedException.none();

    @Before
    public void setUp()
    {
        _sslWrapper = new SimpleSslTransportWrapper(_dummySslEngine, _underlyingInput, _underlyingOutput);
    }

    @Test
    public void testInputDecodesOnePacket()
    {
        String encodedBytes = "<-A->";

        putBytesIntoTransport(encodedBytes);

        assertEquals("a_", _underlyingInput.getAcceptedInput());
    }

    @Test
    public void testInputWithMultiplePackets()
    {
        String encodedBytes = "<-A-><-B-><-C-><>";

        putBytesIntoTransport(encodedBytes);

        assertEquals("a_b_c_z_", _underlyingInput.getAcceptedInput());
    }

    @Test
    public void testInputIncompletePacket_isNotPassedToUnderlyingInputUntilCompleted()
    {
        String incompleteEncodedBytes = "<-A-><-B-><-C"; // missing the trailing '>' to cause the underflow
        String remainingEncodedBytes = "-><-D->";

        putBytesIntoTransport(incompleteEncodedBytes);
        assertEquals("a_b_", _underlyingInput.getAcceptedInput());

        putBytesIntoTransport(remainingEncodedBytes);
        assertEquals("a_b_c_d_", _underlyingInput.getAcceptedInput());
    }

    /**
     * As per {@link #testInputIncompletePacket_isNotPassedToUnderlyingInputUntilCompleted()}
     * but this time it takes TWO chunks to complete the "dangling" packet.
     */
    @Test
    public void testInputIncompletePacketInThreeParts()
    {
        String firstEncodedBytes = "<-A-><-B-><-";
        String secondEncodedBytes = "C"; // Sending this causes the impl to have to hold the data without producing more input yet
        String thirdEncodedBytes = "-><-D->";

        putBytesIntoTransport(firstEncodedBytes);
        assertEquals("a_b_", _underlyingInput.getAcceptedInput());

        putBytesIntoTransport(secondEncodedBytes);
        assertEquals("a_b_", _underlyingInput.getAcceptedInput());

        putBytesIntoTransport(thirdEncodedBytes);
        assertEquals("a_b_c_d_", _underlyingInput.getAcceptedInput());
    }

    @Test
    public void testUnderlyingInputUsingSmallBuffer_receivesAllDecodedInput() throws Exception
    {
        _underlyingInput.setInputBufferSize(1);

        putBytesIntoTransport("<-A->");

        assertEquals("a_", _underlyingInput.getAcceptedInput());
    }

    @Test
    public void testSslUnwrapThrowsException_returnsErrorResultAndRefusesFurtherInput() throws Exception
    {
        SSLException sslException = new SSLException("unwrap exception");
        _dummySslEngine.rejectNextEncodedPacket(sslException);

        _sslWrapper.tail().put("<-A->".getBytes());
        _sslWrapper.process();
        assertEquals(_sslWrapper.capacity(), Transport.END_OF_STREAM);
    }

    @Test
    public void testUnderlyingInputReturnsErrorResult_returnsErrorResultAndRefusesFurtherInput() throws Exception
    {
        String underlyingErrorDescription = "dummy underlying error";
        _underlyingInput.rejectNextInput(underlyingErrorDescription);

        _sslWrapper.tail().put("<-A->".getBytes());

        try {
            _sslWrapper.process();
            fail("no exception");
        } catch (TransportException e) {
            assertEquals(underlyingErrorDescription, e.getMessage());
        }
    }

    @Test
    public void testHeadIsReadOnly()
    {
        _underlyingOutput.setOutput("");
        assertTrue(_sslWrapper.head().isReadOnly());
    }

    @Test
    public void testOutputEncodesOnePacket()
    {
        _underlyingOutput.setOutput("a_");

        ByteBuffer outputBuffer = _sslWrapper.head();

        assertByteBufferContentEquals("<-A->".getBytes(), outputBuffer);
    }

    @Test
    public void testOutputEncodesMultiplePackets()
    {
        _underlyingOutput.setOutput("a_b_c_");

        assertEquals("<-A-><-B-><-C->", getAllBytesFromTransport());
    }

    @Test
    public void testOutputEncodesMultiplePacketsOfVaryingSize()
    {
        _underlyingOutput.setOutput("z_a_b_");

        assertEquals("<><-A-><-B->", getAllBytesFromTransport());
    }

    @Test
    public void testClientConsumesEncodedOutputInMultipleChunks()
    {
        _underlyingOutput.setOutput("a_b_");

        {
            ByteBuffer buffer = _sslWrapper.head();
            String output = pourBufferToString(buffer, 2);
            assertEquals("<-", output);
            _sslWrapper.pop(buffer.position());
        }

        {
            ByteBuffer buffer = _sslWrapper.head();
            String output = pourBufferToString(buffer, 3);
            assertEquals("A->", output);
            _sslWrapper.pop(buffer.position());
        }

        assertEquals("<-B->", getAllBytesFromTransport());
    }

    @Test
    public void testNoOutputToEncode()
    {
        _underlyingOutput.setOutput("");

        assertFalse(_sslWrapper.head().hasRemaining());
    }

    private void putBytesIntoTransport(String encodedBytes)
    {
        ByteBuffer byteBuffer = ByteBuffer.wrap(encodedBytes.getBytes());
        while(byteBuffer.hasRemaining())
        {
            int numberPoured = pour(byteBuffer, _sslWrapper.tail());
            assertTrue("We should be able to pour some bytes into the input buffer",
                    numberPoured > 0);
            _sslWrapper.process();
        }
    }

    private String getAllBytesFromTransport()
    {
        StringBuilder readBytes = new StringBuilder();
        while (true)
        {
            int pending = _sslWrapper.pending();
            if (pending > 0) {
                ByteBuffer buffer = _sslWrapper.head();
                readBytes.append(pourBufferToString(buffer));
                _sslWrapper.pop(pending);
                continue;
            } else {
                break;
            }
        }

        return readBytes.toString();
    }

}
