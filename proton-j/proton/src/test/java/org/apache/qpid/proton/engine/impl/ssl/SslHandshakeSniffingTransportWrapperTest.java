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

import static org.apache.qpid.proton.engine.impl.TransportTestHelper.assertByteBufferContentEquals;
import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.Mockito.when;

import java.nio.ByteBuffer;

import org.apache.qpid.proton.engine.impl.TransportWrapper;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;

public class SslHandshakeSniffingTransportWrapperTest
{
    private static final byte[] EXAMPLE_SSL_V3_HANDSHAKE_BYTES = new byte[] {0x16, 0x03, 0x02, 0x00, 0x31};
    private static final byte[] EXAMPLE_SSL_V2_HANDSHAKE_BYTES = new byte[] {0x00, 0x00, 0x01, 0x03, 0x00};

    private SslTransportWrapper _secureTransportWrapper = mock(SslTransportWrapper.class);
    private TransportWrapper _plainTransportWrapper = mock(TransportWrapper.class);
    private SslTransportWrapper _sniffingWrapper = new SslHandshakeSniffingTransportWrapper(_secureTransportWrapper, _plainTransportWrapper);

    @Rule
    public ExpectedException _expectedException = ExpectedException.none();

    @Test
    public void testGetInputBufferGetOutputBufferWithNonSsl()
    {
        testInputAndOutput("INPUT".getBytes(), _plainTransportWrapper);
    }

    @Test
    public void testWithSSLv2()
    {
        testInputAndOutput(EXAMPLE_SSL_V2_HANDSHAKE_BYTES, _secureTransportWrapper);
    }

    @Test
    public void testWithSSLv3TLS()
    {
        testInputAndOutput(EXAMPLE_SSL_V3_HANDSHAKE_BYTES, _secureTransportWrapper);
    }

    private void testInputAndOutput(byte[] input, TransportWrapper transportThatShouldBeUsed)
    {
        byte[] output = "OUTPUT".getBytes();

        ByteBuffer underlyingInputBuffer = ByteBuffer.allocate(1024);
        ByteBuffer underlyingOutputBuffer = ByteBuffer.wrap(output);

        // set up underlying transport
        when(transportThatShouldBeUsed.getInputBuffer()).thenReturn(underlyingInputBuffer);
        when(transportThatShouldBeUsed.getOutputBuffer()).thenReturn(underlyingOutputBuffer);

        // do input and verify underlying calls were made
        ByteBuffer inputBuffer = _sniffingWrapper.getInputBuffer();
        inputBuffer.put(input);
        _sniffingWrapper.processInput();

        verify(transportThatShouldBeUsed).getInputBuffer();
        verify(transportThatShouldBeUsed).processInput();

        // check the wrapped input actually received the expected bytes
        underlyingInputBuffer.flip();
        assertByteBufferContentEquals(input, underlyingInputBuffer);

        // do output and check we get the correct transport's output
        ByteBuffer outputBuffer = _sniffingWrapper.getOutputBuffer();
        verify(transportThatShouldBeUsed).getOutputBuffer();

        assertByteBufferContentEquals(output, outputBuffer);
        _sniffingWrapper.outputConsumed();
        verify(transportThatShouldBeUsed).outputConsumed();

        verifyZeroInteractionsWithOtherTransport(transportThatShouldBeUsed);
    }

    @Test
    public void testTooFewBytesToMakeDetermination()
    {
        byte[] sourceBuffer = new byte[] {0x00};

        try
        {
            _sniffingWrapper.getInputBuffer().put(sourceBuffer);

            _expectedException.expect(IllegalArgumentException.class);
            _sniffingWrapper.processInput();
        }
        finally
        {
            verifyZeroInteractions(_secureTransportWrapper, _plainTransportWrapper);
        }
    }

    @Test
    public void testGetSslAttributesWhenProtocolIsNotYetDetermined_returnNull()
    {
        assertEquals("Cipher name should be null", null, _sniffingWrapper.getCipherName());
        assertEquals("Protocol name should be null", null, _sniffingWrapper.getProtocolName());
        verifyZeroInteractions(_secureTransportWrapper, _plainTransportWrapper);
    }

    @Test
    public void testGetSslAttributesWhenUsingNonSsl_returnNull()
    {
        testGetSslAttributes("INPUT".getBytes(), _plainTransportWrapper, null, null);
    }

    /**
     * Tests {@link SslHandshakeSniffingTransportWrapper#getCipherName()}
     * and {@link SslHandshakeSniffingTransportWrapper#getProtocolName()}.
     */
    @Test
    public void testGetSslAttributesWhenUsingSsl()
    {
        String cipherName = "testCipherName";
        String protocolName = "testProtocolName";
        when(_secureTransportWrapper.getCipherName()).thenReturn(cipherName);
        when(_secureTransportWrapper.getProtocolName()).thenReturn(protocolName);

        testGetSslAttributes(EXAMPLE_SSL_V2_HANDSHAKE_BYTES, _secureTransportWrapper, cipherName, protocolName);
    }

    private void testGetSslAttributes(
            byte[] input, TransportWrapper transportThatShouldBeUsed,
            String expectedCipherName, String expectedProtocolName)
    {
        ByteBuffer underlyingInputBuffer = ByteBuffer.allocate(1024);
        when(transportThatShouldBeUsed.getInputBuffer()).thenReturn(underlyingInputBuffer);

        _sniffingWrapper.getInputBuffer().put(input);
        _sniffingWrapper.processInput();

        assertEquals(expectedCipherName, _sniffingWrapper.getCipherName());
        assertEquals(expectedProtocolName, _sniffingWrapper.getProtocolName());

        verifyZeroInteractionsWithOtherTransport(transportThatShouldBeUsed);
    }

    private void verifyZeroInteractionsWithOtherTransport(TransportWrapper transportThatShouldBeUsed)
    {
        final TransportWrapper transportThatShouldNotBeUsed;
        if(transportThatShouldBeUsed == _plainTransportWrapper)
        {
            transportThatShouldNotBeUsed = _secureTransportWrapper;
        }
        else
        {
            transportThatShouldNotBeUsed = _plainTransportWrapper;
        }

        verifyZeroInteractions(transportThatShouldNotBeUsed);
    }

}
