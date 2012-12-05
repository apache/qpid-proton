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

import static org.apache.qpid.proton.engine.impl.ByteTestHelper.assertArrayUntouchedExcept;
import static org.apache.qpid.proton.engine.impl.ByteTestHelper.createFilledBuffer;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import org.apache.qpid.proton.engine.Ssl;
import org.apache.qpid.proton.engine.impl.SimpleSslTransportWrapper;
import org.apache.qpid.proton.engine.impl.SslEngineFacadeFactory;
import org.junit.Before;
import org.junit.Test;

/**
 * TODO unit test handshaking
 * TODO unit test closing
 */
public class SimpleSslTransportWrapperTest
{
    private Ssl _sslConfiguration = mock(Ssl.class);
    private SslEngineFacadeFactory _dummySslEngineFacadeFactory = mock(SslEngineFacadeFactory.class);

    private RememberingTransportInput _underlyingInput = new RememberingTransportInput();
    private CannedTransportOutput _underlyingOutput = new CannedTransportOutput();

    private SimpleSslTransportWrapper _transportWrapper;

    private CapitalisingDummySslEngine _dummySslEngine = new CapitalisingDummySslEngine();

    @Before
    public void setUp()
    {
        when(_dummySslEngineFacadeFactory.createSslEngineFacade(_sslConfiguration)).thenReturn(_dummySslEngine);
        _transportWrapper = new SimpleSslTransportWrapper(_sslConfiguration, _underlyingInput, _underlyingOutput, _dummySslEngineFacadeFactory);
    }

    @Test
    public void testInputDecodesOnePacket()
    {
        byte[] encodedBytes = "<-A->".getBytes();

        int numberConsumed = _transportWrapper.input(encodedBytes, 0, encodedBytes.length);
        assertEquals(encodedBytes.length, numberConsumed);
        assertEquals("a_", _underlyingInput.getAcceptedInput());
    }

    @Test
    public void testInputWithMultiplePackets()
    {
        byte[] encodedBytes = "<-A-><-B-><-C-><>".getBytes();

        int numberConsumed = _transportWrapper.input(encodedBytes, 0, encodedBytes.length);
        assertEquals(encodedBytes.length, numberConsumed);
        assertEquals("a_b_c_z_", _underlyingInput.getAcceptedInput());
    }

    @Test
    public void testInputWithMultiplePacketsUsingNonZeroOffset()
    {
        byte[] encodedBytes = "<-A-><-B-><-C-><-D-><-E->".getBytes();

        // try to decode the "<-B-><-C->" portion of encodedBytes
        int numberConsumed = _transportWrapper.input(encodedBytes, 5, 10);
        assertEquals(10, numberConsumed);
        assertEquals("b_c_", _underlyingInput.getAcceptedInput());
    }

    @Test
    public void testInsufficientInputBufferUnderflow()
    {
        byte[] incompleteEncodedBytes = "<-A-><-B-><-C".getBytes(); // missing the trailing '>' to cause the underflow
        byte[] remainingEncodedBytes = "-><-D->".getBytes();

        int numberConsumed = _transportWrapper.input(incompleteEncodedBytes, 0, incompleteEncodedBytes.length);
        assertEquals(incompleteEncodedBytes.length, numberConsumed);
        assertEquals("a_b_", _underlyingInput.getAcceptedInput());

        numberConsumed = _transportWrapper.input(remainingEncodedBytes, 0, remainingEncodedBytes.length);
        assertEquals(remainingEncodedBytes.length, numberConsumed);
        assertEquals("a_b_c_d_", _underlyingInput.getAcceptedInput());
    }

    @Test
    public void testInsufficientInputBufferUnderflowThreePart()
    {
        byte[] firstEncodedBytes = "<-A-><-B-><-".getBytes();
        byte[] secondEncodedBytes = "C".getBytes(); // Sending this causes the impl to have to hold the data without producing more input yet
        byte[] thirdEncodedBytes = "-><-D->".getBytes();

        int numberConsumed = _transportWrapper.input(firstEncodedBytes, 0, firstEncodedBytes.length);
        assertEquals(firstEncodedBytes.length, numberConsumed);
        assertEquals("a_b_", _underlyingInput.getAcceptedInput());

        numberConsumed = _transportWrapper.input(secondEncodedBytes, 0, secondEncodedBytes.length);
        assertEquals(secondEncodedBytes.length, numberConsumed);
        assertEquals("a_b_", _underlyingInput.getAcceptedInput());

        numberConsumed = _transportWrapper.input(thirdEncodedBytes, 0, thirdEncodedBytes.length);
        assertEquals(thirdEncodedBytes.length, numberConsumed);
        assertEquals("a_b_c_d_", _underlyingInput.getAcceptedInput());
    }

    @Test
    public void testUnderlyingInputBecomesTemporarilyFull()
    {
        _underlyingInput.setAcceptLimit(3);

        byte[] encodedBytes = "<-A-><-B-><-C->".getBytes();

        // We consume encoded A and B, but due to the limit underlying input accepts
        // only a part of b's decoded packet.
        int firstNumberConsumed = _transportWrapper.input(encodedBytes, 0, encodedBytes.length);
        assertEquals(10, firstNumberConsumed);
        assertEquals("a_b", _underlyingInput.getAcceptedInput());

        _underlyingInput.removeAcceptLimit();

        // Send the remaining encoded data
        int remainingOffset = firstNumberConsumed;
        int remainingSize = encodedBytes.length - firstNumberConsumed;
        int secondNumberConsumed = _transportWrapper.input(encodedBytes, remainingOffset, remainingSize);
        assertEquals(5, secondNumberConsumed);
        assertEquals("a_b_c_", _underlyingInput.getAcceptedInput());
    }

    /**
     * @see #testUnderlyingInputBecomesTemporarilyFull()
     */
    @Test
    public void testUnderlyingInputBecomesPermanentlyFull()
    {
        _underlyingInput.setAcceptLimit(3);

        byte[] encodedBytes = "<-A-><-B-><-C->".getBytes();

        int firstNumberConsumed = _transportWrapper.input(encodedBytes, 0, encodedBytes.length);
        assertEquals(10, firstNumberConsumed);
        assertEquals("a_b", _underlyingInput.getAcceptedInput());

        // Send the remaining encoded data
        int remainingOffset = firstNumberConsumed;
        int remainingSize = encodedBytes.length - firstNumberConsumed;
        int secondNumberConsumed = _transportWrapper.input(encodedBytes, remainingOffset, remainingSize);
        assertEquals(0, secondNumberConsumed);
        assertEquals("a_b", _underlyingInput.getAcceptedInput());
    }

    public void testUnderlyingInputPartiallyAcceptsLeftovers()
    {
        byte[] encodedBytes = "<A><B><C>".getBytes();

        _underlyingInput.setAcceptLimit(0);

        int firstNumberConsumed = _transportWrapper.input(encodedBytes, 0, encodedBytes.length);
        assertEquals(3, firstNumberConsumed);
        assertEquals("", _underlyingInput.getAcceptedInput());

        // Set underlying input to accept *part* of the decoded leftovers, then try to send the remaining encoded data
        _underlyingInput.setAcceptLimit(1);
        int offsetAfterFirstAttempt = firstNumberConsumed;
        int sizeAfterFirstAttempt = encodedBytes.length - firstNumberConsumed;
        int secondNumberConsumed = _transportWrapper.input(encodedBytes, offsetAfterFirstAttempt, sizeAfterFirstAttempt);
        assertEquals(0, secondNumberConsumed);
        assertEquals("a", _underlyingInput.getAcceptedInput());

        // Remove the limit and send the remaining data.
        _underlyingInput.removeAcceptLimit();
        // offset and size unchanged because second attempt consumed no bytes
        int thirdNumberConsumed = _transportWrapper.input(encodedBytes, offsetAfterFirstAttempt, sizeAfterFirstAttempt);
        assertEquals(6, thirdNumberConsumed);
        assertEquals("a_b_c_", _underlyingInput.getAcceptedInput());
    }

    @Test
    public void testOutputEncodesOnePacket()
    {
        byte[] encodedBytes = createFilledBuffer(10);
        String expectedOutputProduced = "<-A->";

        _underlyingOutput.setOutput("a_");

        int numberProduced = _transportWrapper.output(encodedBytes, 0, encodedBytes.length);
        assertEquals(expectedOutputProduced.length(), numberProduced);
        assertArrayUntouchedExcept(expectedOutputProduced, encodedBytes);
    }

    @Test
    public void testOutputEncodesOnePacketUsingNonZeroOffset()
    {
        byte[] encodedBytes = createFilledBuffer(10);
        String expectedOutputProduced = "<-A->";

        _underlyingOutput.setOutput("a_");

        int numberProduced = _transportWrapper.output(encodedBytes, 1, 5);
        assertEquals(expectedOutputProduced.length(), numberProduced);
        assertArrayUntouchedExcept(expectedOutputProduced, encodedBytes, 1);
    }

    @Test
    public void testOutputUsingSmallBuffers()
    {
        String expectedOutputProducedFirstAttempt = "<-";
        String expectedOutputProducedSecondAttempt = "A";
        String expectedOutputProducedThirdAttempt = "->";

        String expectedOutputProducedLastAttempt = "<-B->";

        _underlyingOutput.setOutput("a_");
        {
            byte[] encodedBytesForFirstAttempt = createFilledBuffer(2);
            int numberProducedFirstAttempt = _transportWrapper.output(encodedBytesForFirstAttempt, 0, encodedBytesForFirstAttempt.length);
            assertEquals(expectedOutputProducedFirstAttempt.length(), numberProducedFirstAttempt);
            assertArrayUntouchedExcept(expectedOutputProducedFirstAttempt, encodedBytesForFirstAttempt);
        }

        {
            byte[] encodedBytesForSecondAttempt = createFilledBuffer(1);
            int numberProducedSecondAttempt = _transportWrapper.output(encodedBytesForSecondAttempt, 0, encodedBytesForSecondAttempt.length);
            assertEquals(expectedOutputProducedSecondAttempt.length(), numberProducedSecondAttempt);
            assertArrayUntouchedExcept(expectedOutputProducedSecondAttempt, encodedBytesForSecondAttempt);
        }

        {
            byte[] encodedBytesForThirdAttempt = createFilledBuffer(2);
            int numberProducedThirdAttempt = _transportWrapper.output(encodedBytesForThirdAttempt, 0, encodedBytesForThirdAttempt.length);
            assertEquals(expectedOutputProducedThirdAttempt.length(), numberProducedThirdAttempt);
            assertArrayUntouchedExcept(expectedOutputProducedThirdAttempt, encodedBytesForThirdAttempt);
        }

        _underlyingOutput.setOutput("b_");
        {
            byte[] encodedBytesForLastAttempt = createFilledBuffer(10);
            int numberProducedLastAttempt = _transportWrapper.output(encodedBytesForLastAttempt, 0, encodedBytesForLastAttempt.length);
            assertEquals(expectedOutputProducedLastAttempt.length(), numberProducedLastAttempt);
            assertArrayUntouchedExcept(expectedOutputProducedLastAttempt, encodedBytesForLastAttempt);
        }
    }

    @Test
    public void testOutputUsingSmallBuffersAndDecodingMoreBytesAlongTheWay()
    {
        String expectedOutputProducedFirstAttempt = "<-";
        String expectedOutputProducedSecondAttempt = "A-";

        String expectedOutputProducedThirdAttempt = "><-B->";
        String expectedOutputProducedLastAttempt = "<-C->";


        _underlyingOutput.setOutput("a_");
        {
            byte[] encodedBytesForFirstAttempt = createFilledBuffer(2);
            int numberProducedFirstAttempt = _transportWrapper.output(encodedBytesForFirstAttempt, 0, encodedBytesForFirstAttempt.length);
            assertEquals(expectedOutputProducedFirstAttempt.length(), numberProducedFirstAttempt);
            assertArrayUntouchedExcept(expectedOutputProducedFirstAttempt, encodedBytesForFirstAttempt);
        }

        {
            byte[] encodedBytesForSecondAttempt = createFilledBuffer(2);
            int numberProducedSecondAttempt = _transportWrapper.output(encodedBytesForSecondAttempt, 0, encodedBytesForSecondAttempt.length);
            assertEquals(expectedOutputProducedSecondAttempt.length(), numberProducedSecondAttempt);
            assertArrayUntouchedExcept(expectedOutputProducedSecondAttempt, encodedBytesForSecondAttempt);
        }

        {
            // now get some output into a roomy buffer, which should get the left-over ">" plus some new stuff
            _underlyingOutput.setOutput("b_");
            byte[] encodedBytesForThirdAttempt = createFilledBuffer(10);
            int numberProducedThirdAttempt = _transportWrapper.output(encodedBytesForThirdAttempt, 0, encodedBytesForThirdAttempt.length);
            assertEquals(expectedOutputProducedThirdAttempt.length(), numberProducedThirdAttempt);
            assertArrayUntouchedExcept(expectedOutputProducedThirdAttempt, encodedBytesForThirdAttempt);
        }

        _underlyingOutput.setOutput("c_");
        {
            byte[] encodedBytesForLastAttempt = createFilledBuffer(10);
            int numberProducedLastAttempt = _transportWrapper.output(encodedBytesForLastAttempt, 0, encodedBytesForLastAttempt.length);
            assertEquals(expectedOutputProducedLastAttempt.length(), numberProducedLastAttempt);
            assertArrayUntouchedExcept(expectedOutputProducedLastAttempt, encodedBytesForLastAttempt);
        }
    }

    @Test
    public void testOutputEncodesMultiplePackets()
    {
        byte[] encodedBytes = createFilledBuffer(17);
        String expectedOutputProduced = "<-A-><-B-><-C-><>";

        _underlyingOutput.setOutput("a_b_c_z_");

        int numberProduced = _transportWrapper.output(encodedBytes, 0, encodedBytes.length);
        assertEquals(expectedOutputProduced.length(), numberProduced);
        assertArrayUntouchedExcept(expectedOutputProduced, encodedBytes);
    }

    @Test
    public void testOutputWritesShortEncodedPacketIntoBufferThatIsLessThanMaximumPacketSize()
    {
        int bufferLength = _dummySslEngine.getPacketBufferSize() - 1;
        assertTrue(bufferLength > CapitalisingDummySslEngine.SHORT_ENCODED_CHUNK_SIZE);

        _underlyingOutput.setOutput("z_");

        {
            byte[] encodedBytes = createFilledBuffer(bufferLength); // smaller than a packet but large enough to receive encoded bytes for z_
            String expectedOutputProduced = "<>";
            int numberProduced = _transportWrapper.output(encodedBytes, 0, encodedBytes.length);
            assertEquals(expectedOutputProduced.length(), numberProduced);
            assertArrayUntouchedExcept(expectedOutputProduced, encodedBytes);
        }
    }

    @Test
    public void testOutputEncodesMultiplePacketsWithInitialBufferTooSmallForAllOfSecondPacket()
    {
        String expectedOutputProducedFirstAttempt = "<-A-><";
        String expectedOutputProducedSecondAttempt = "><-C->";

        _underlyingOutput.setOutput("a_z_c_");

        {
            byte[] encodedBytesFirstAttempt = createFilledBuffer(6);
            int numberProducedFirstAttempt = _transportWrapper.output(encodedBytesFirstAttempt, 0, encodedBytesFirstAttempt.length);
            assertEquals(expectedOutputProducedFirstAttempt.length(), numberProducedFirstAttempt);
            assertArrayUntouchedExcept(expectedOutputProducedFirstAttempt, encodedBytesFirstAttempt);
        }
        {
            byte[] encodedBytesSecondAttempt = createFilledBuffer(6);
            int numberProducedSecondAttempt = _transportWrapper.output(encodedBytesSecondAttempt, 0, encodedBytesSecondAttempt.length);
            assertEquals(expectedOutputProducedSecondAttempt.length(), numberProducedSecondAttempt);
            assertArrayUntouchedExcept(expectedOutputProducedSecondAttempt, encodedBytesSecondAttempt);
        }
    }

    @Test
    public void testNoOutputToEncode()
    {
        byte[] encodedBytes = createFilledBuffer(10);
        String expectedOutputProduced = "";

        _underlyingOutput.setOutput("");

        int numberProduced = _transportWrapper.output(encodedBytes, 0, encodedBytes.length);
        assertEquals(expectedOutputProduced.length(), numberProduced);
        assertArrayUntouchedExcept(expectedOutputProduced, encodedBytes);
    }
}
