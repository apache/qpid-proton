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
import java.nio.charset.StandardCharsets;

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
    private RememberingTransportInput _underlyingInput;
    private CannedTransportOutput _underlyingOutput;
    private SimpleSslTransportWrapper _sslWrapper;
    private CapitalisingDummySslEngine _dummySslEngine;

    @Rule
    public ExpectedException _expectedException = ExpectedException.none();

    @Before
    public void setUp()
    {
        _underlyingInput = new RememberingTransportInput();
        _underlyingOutput = new CannedTransportOutput();
        _dummySslEngine = new CapitalisingDummySslEngine();
        _sslWrapper = new SimpleSslTransportWrapper(_dummySslEngine, _underlyingInput, _underlyingOutput);
    }

    @Test
    public void testInputDecodesOnePacket()
    {
        String encodedBytes = "<-A->";

        putBytesIntoTransport(encodedBytes);

        assertEquals("a_", _underlyingInput.getAcceptedInput());
        assertEquals(CapitalisingDummySslEngine.MAX_ENCODED_CHUNK_SIZE, _sslWrapper.capacity());
        assertEquals(2, _dummySslEngine.getUnwrapCount());// 1 packet, 1 underflow
        assertEquals(1, _underlyingInput.getProcessCount());
    }

    /**
     * Note that this only feeds 1 encoded packet in at a time due to default settings of the dummy engine,
     * See {@link #testUnderlyingInputUsingSmallBuffer_receivesAllDecodedInputRequiringMultipleUnwraps}
     * for a related test that passes multiple encoded packets into the ssl wrapper at once.
     */
    @Test
    public void testInputWithMultiplePackets()
    {
        String encodedBytes = "<-A-><-B-><-C-><>";

        putBytesIntoTransport(encodedBytes);

        assertEquals("a_b_c_z_", _underlyingInput.getAcceptedInput());
        assertEquals(CapitalisingDummySslEngine.MAX_ENCODED_CHUNK_SIZE, _sslWrapper.capacity());
        assertEquals(8, _dummySslEngine.getUnwrapCount()); // (1 decode + 1 underflow) * 4 packets
        assertEquals(4, _underlyingInput.getProcessCount()); // 1 process per decoded packet
    }

    @Test
    public void testInputIncompletePacket_isNotPassedToUnderlyingInputUntilCompleted()
    {
        String incompleteEncodedBytes = "<-A-><-B-><-C"; // missing the trailing '>' to cause the underflow
        String remainingEncodedBytes = "-><-D->";

        putBytesIntoTransport(incompleteEncodedBytes);
        assertEquals("a_b_", _underlyingInput.getAcceptedInput());
        assertEquals(5, _dummySslEngine.getUnwrapCount()); // 2 * (1 decode + 1 underflow) + 1 underflow
        assertEquals(2, _underlyingInput.getProcessCount()); // 1 process per decoded packet

        putBytesIntoTransport(remainingEncodedBytes);
        assertEquals("a_b_c_d_", _underlyingInput.getAcceptedInput());
        assertEquals(4, _underlyingInput.getProcessCount()); // earlier + 2
        assertEquals(9, _dummySslEngine.getUnwrapCount()); // Earlier + 2 * (1 decode + 1 underflow)
                                                           // due to way the bytes are fed in across
                                                           // boundary of encoded packets
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
        assertEquals(5, _dummySslEngine.getUnwrapCount()); // 2 * (1 decode + 1 underflow) + 1 underflow
        assertEquals(2, _underlyingInput.getProcessCount()); // 1 process per decoded packet

        putBytesIntoTransport(secondEncodedBytes);
        assertEquals("a_b_", _underlyingInput.getAcceptedInput());
        assertEquals(6, _dummySslEngine.getUnwrapCount()); // earlier + 1 underflow
        assertEquals(2, _underlyingInput.getProcessCount()); // as earlier

        putBytesIntoTransport(thirdEncodedBytes);
        assertEquals("a_b_c_d_", _underlyingInput.getAcceptedInput());
        assertEquals(4, _underlyingInput.getProcessCount()); // 1 process per decoded packet
        assertEquals(10, _dummySslEngine.getUnwrapCount()); // Earlier + (decode + underflow) * 2
                                                           // due to way the bytes are fed in across
                                                           // boundary of encoded packets
    }

    /**
     * Tests that when a small underlying input buffer (1 byte here) is used, all of the encoded
     * data packet (5 bytes each here) can be processed despite multiple attempts being required to
     * pass the decoded bytes (2 bytes here) to the underlying input layer for processing.
     */
    @Test
    public void testUnderlyingInputUsingSmallBuffer_receivesAllDecodedInputRequiringMultipleUnderlyingProcesses()
    {
        int underlyingInputBufferSize = 1;
        int encodedPacketSize = 5;

        _underlyingInput.setInputBufferSize(underlyingInputBufferSize);
        assertEquals("Unexpected underlying input capacity", underlyingInputBufferSize, _underlyingInput.capacity());

        assertEquals("Unexpected max encoded chunk size", encodedPacketSize, CapitalisingDummySslEngine.MAX_ENCODED_CHUNK_SIZE);

        byte[] bytes = "<-A-><-B->".getBytes(StandardCharsets.UTF_8);
        ByteBuffer encodedByteSource = ByteBuffer.wrap(bytes);

        assertEquals("Unexpected initial capacity", encodedPacketSize, _sslWrapper.capacity());

        // Process the first 'encoded packet' (<-A->)
        int numberPoured = pour(encodedByteSource, _sslWrapper.tail());
        assertEquals("Unexpected number of bytes poured into the wrapper input buffer", encodedPacketSize, numberPoured);
        assertEquals("Unexpected position in encoded source byte buffer", encodedPacketSize * 1, encodedByteSource.position());
        assertEquals("Unexpected capacity", 0, _sslWrapper.capacity());
        _sslWrapper.process();
        assertEquals("Unexpected capacity", encodedPacketSize, _sslWrapper.capacity());

        assertEquals("unexpected underlying output after first wrapper process", "a_", _underlyingInput.getAcceptedInput());
        assertEquals("unexpected underlying process count after first wrapper process", 2 , _underlyingInput.getProcessCount());

        // Process the second 'encoded packet' (<-B->)
        numberPoured = pour(encodedByteSource, _sslWrapper.tail());
        assertEquals("Unexpected number of bytes poured into the wrapper input buffer", encodedPacketSize, numberPoured);
        assertEquals("Unexpected position in encoded source byte buffer", encodedPacketSize * 2, encodedByteSource.position());
        assertEquals("Unexpected capacity", 0, _sslWrapper.capacity());
        _sslWrapper.process();
        assertEquals("Unexpected capacity", encodedPacketSize, _sslWrapper.capacity());

        assertEquals("unexpected underlying output after second wrapper process", "a_b_", _underlyingInput.getAcceptedInput());
        assertEquals("unexpected underlying process count after second wrapper process", 4 , _underlyingInput.getProcessCount());
    }

    /**
     * Tests that when a small underlying input buffer (1 byte here) is used, all of the encoded
     * data packets (20 bytes total here) can be processed despite multiple unwraps being required
     * to process a given set of input (3 packets, 15 bytes here) and then as a result also multiple
     * attempts to pass the decoded packet (2 bytes here) to the underlying input layer for processing.
     */
    @Test
    public void testUnderlyingInputUsingSmallBuffer_receivesAllDecodedInputRequiringMultipleUnwraps()
    {
        int underlyingInputBufferSize = 1;
        int encodedPacketSize = 5;
        int sslEngineBufferSize = 15;

        assertEquals("Unexpected max encoded chunk size", encodedPacketSize, CapitalisingDummySslEngine.MAX_ENCODED_CHUNK_SIZE);

        _underlyingOutput = new CannedTransportOutput();
        _underlyingInput = new RememberingTransportInput();
        _underlyingInput.setInputBufferSize(underlyingInputBufferSize);
        assertEquals("Unexpected underlying input capacity", underlyingInputBufferSize, _underlyingInput.capacity());

        // Create a dummy ssl engine that has buffers that holds multiple encoded/decoded
        // packets, but still can't fit all of the input
        _dummySslEngine = new CapitalisingDummySslEngine();
        _dummySslEngine.setApplicationBufferSize(sslEngineBufferSize);
        _dummySslEngine.setPacketBufferSize(sslEngineBufferSize);

        _sslWrapper = new SimpleSslTransportWrapper(_dummySslEngine, _underlyingInput, _underlyingOutput);

        byte[] bytes = "<-A-><-B-><-C-><-D->".getBytes(StandardCharsets.UTF_8);
        ByteBuffer encodedByteSource = ByteBuffer.wrap(bytes);

        assertEquals("Unexpected initial capacity", sslEngineBufferSize, _sslWrapper.capacity());

        // Process the first three 'encoded packets' (<-A-><-B-><-C->). This will require 3 'proper' unwraps, and
        // as each decoded packet is 2 bytes, each of those will require 2 underlying input processes.
        int numberPoured = pour(encodedByteSource, _sslWrapper.tail());
        assertEquals("Unexpected number of bytes poured into the wrapper input buffer", sslEngineBufferSize, numberPoured);
        assertEquals("Unexpected position in encoded source byte buffer", encodedPacketSize * 3, encodedByteSource.position());
        assertEquals("Unexpected capacity", 0, _sslWrapper.capacity());

        _sslWrapper.process();

        assertEquals("a_b_c_", _underlyingInput.getAcceptedInput());
        assertEquals("Unexpected capacity", sslEngineBufferSize, _sslWrapper.capacity());
        assertEquals("unexpected underlying process count after wrapper process", 6 , _underlyingInput.getProcessCount());
        assertEquals(4, _dummySslEngine.getUnwrapCount()); // 3 decodes + 1 underflow

        // Process the fourth 'encoded packet' (<-D->)
        numberPoured = pour(encodedByteSource, _sslWrapper.tail());
        assertEquals("Unexpected number of bytes poured into the wrapper input buffer", encodedPacketSize, numberPoured);
        assertEquals("Unexpected position in encoded source byte buffer", encodedPacketSize * 4, encodedByteSource.position());
        assertEquals("Unexpected capacity", sslEngineBufferSize - encodedPacketSize, _sslWrapper.capacity());

        _sslWrapper.process();

        assertEquals("a_b_c_d_", _underlyingInput.getAcceptedInput());
        assertEquals("Unexpected capacity", sslEngineBufferSize, _sslWrapper.capacity());
        assertEquals("unexpected underlying process count after second wrapper process", 8 , _underlyingInput.getProcessCount());
        assertEquals(6, _dummySslEngine.getUnwrapCount()); // earlier + 1 decode + 1 underflow
    }

    /**
     * Tests that an exception is thrown when the underlying input has zero capacity when the call
     * with newly decoded input is initially made.
     */
    @Test (timeout = 5000)
    public void testUnderlyingInputHasZeroCapacityInitially()
    {
        int underlyingInputBufferSize = 1;
        int encodedPacketSize = 5;

        assertEquals("Unexpected max encoded chunk size", encodedPacketSize, CapitalisingDummySslEngine.MAX_ENCODED_CHUNK_SIZE);

        // Set the input to have a small buffer, but then return 0 from the 2nd capacity call onward.
        _underlyingInput.setInputBufferSize(underlyingInputBufferSize);
        _underlyingInput.setZeroCapacityAtCount(2);
        assertEquals("Unexpected initial underlying input capacity", underlyingInputBufferSize, _underlyingInput.capacity());
        assertEquals("Unexpected underlying input capacity", 0, _underlyingInput.capacity());

        // Now try decoding the input, should fail
        byte[] bytes = "<-A->".getBytes(StandardCharsets.UTF_8);
        ByteBuffer encodedByteSource = ByteBuffer.wrap(bytes);

        assertEquals("Unexpected initial wrapper capacity", encodedPacketSize, _sslWrapper.capacity());

        int numberPoured = pour(encodedByteSource, _sslWrapper.tail());
        assertEquals("Unexpected number of bytes poured into the wrapper input buffer", encodedPacketSize, numberPoured);
        assertEquals("Unexpected position in encoded source byte buffer", encodedPacketSize, encodedByteSource.position());
        assertEquals("Unexpected wrapper capacity", 0, _sslWrapper.capacity());

        try
        {
            _sslWrapper.process();
            fail("Expected an exception");
        }
        catch (TransportException te)
        {
            // expected.
        }

        //Check we got no chars of decoded output.
        assertEquals("", _underlyingInput.getAcceptedInput());
        assertEquals("Unexpected wrapper capacity", -1, _sslWrapper.capacity());
        assertEquals("unexpected underlying process count after wrapper process", 0 , _underlyingInput.getProcessCount());
        assertEquals("unexpected underlying capacity count after wrapper process", 3, _underlyingInput.getCapacityCount());
        assertEquals("unexpected underlying capacity after wrapper process", 0 , _underlyingInput.capacity());
        assertEquals(1, _dummySslEngine.getUnwrapCount()); // 1 decode (then exception)
    }

    /**
     * Tests that an exception is thrown when the underlying input has no capacity (but isn't closed)
     * during the process of incrementally passing the decoded bytes to its smaller input buffer
     * for processing.
     */
    @Test (timeout = 5000)
    public void testUnderlyingInputHasZeroCapacityMidProcessing()
    {
        int underlyingInputBufferSize = 1;
        int encodedPacketSize = 5;

        assertEquals("Unexpected max encoded chunk size", encodedPacketSize, CapitalisingDummySslEngine.MAX_ENCODED_CHUNK_SIZE);

        // Set the input to have a small buffer, but then return 0 from the 3rd capacity call onward.
        _underlyingInput.setInputBufferSize(underlyingInputBufferSize);
        _underlyingInput.setZeroCapacityAtCount(3);
        assertEquals("Unexpected initial underlying input capacity", underlyingInputBufferSize, _underlyingInput.capacity());

        // Now try decoding the input, should fail
        byte[] bytes = "<-A->".getBytes(StandardCharsets.UTF_8);
        ByteBuffer encodedByteSource = ByteBuffer.wrap(bytes);

        assertEquals("Unexpected initial wrapper capacity", encodedPacketSize, _sslWrapper.capacity());

        int numberPoured = pour(encodedByteSource, _sslWrapper.tail());
        assertEquals("Unexpected number of bytes poured into the wrapper input buffer", encodedPacketSize, numberPoured);
        assertEquals("Unexpected position in encoded source byte buffer", encodedPacketSize, encodedByteSource.position());
        assertEquals("Unexpected wrapper capacity", 0, _sslWrapper.capacity());

        try
        {
            _sslWrapper.process();
            fail("Expected an exception");
        }
        catch (TransportException te)
        {
            // expected.
        }

        //Check we got the first char (a) of decoded output, but not the second (_).
        assertEquals("a", _underlyingInput.getAcceptedInput());
        assertEquals("Unexpected wrapper capacity", -1, _sslWrapper.capacity());
        assertEquals("unexpected underlying process count after wrapper process", 1 , _underlyingInput.getProcessCount());
        assertEquals("unexpected underlying capacity count after wrapper process", 3, _underlyingInput.getCapacityCount());
        assertEquals("unexpected underlying capacity after wrapper process", 0 , _underlyingInput.capacity());
        assertEquals(1, _dummySslEngine.getUnwrapCount()); // 1 decode (then exception)
    }

    @Test
    public void testSslUnwrapThrowsException_returnsErrorResultAndRefusesFurtherInput() throws Exception
    {
        SSLException sslException = new SSLException("unwrap exception");
        _dummySslEngine.rejectNextEncodedPacket(sslException);

        _sslWrapper.tail().put("<-A->".getBytes(StandardCharsets.UTF_8));
        _sslWrapper.process();
        assertEquals(_sslWrapper.capacity(), Transport.END_OF_STREAM);
    }

    @Test
    public void testUnderlyingInputReturnsErrorResult_returnsErrorResultAndRefusesFurtherInput() throws Exception
    {
        String underlyingErrorDescription = "dummy underlying error";
        _underlyingInput.rejectNextInput(underlyingErrorDescription);

        _sslWrapper.tail().put("<-A->".getBytes(StandardCharsets.UTF_8));

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

        assertByteBufferContentEquals("<-A->".getBytes(StandardCharsets.UTF_8), outputBuffer);
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
        ByteBuffer byteBuffer = ByteBuffer.wrap(encodedBytes.getBytes(StandardCharsets.UTF_8));
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
