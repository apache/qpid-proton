/*
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
 */
package org.apache.qpid.proton.engine.impl;

import static org.mockito.Mockito.*;
import static org.junit.Assert.*;
import static org.junit.matchers.JUnitMatchers.containsString;

import java.nio.ByteBuffer;

import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.security.SaslFrameBody;
import org.apache.qpid.proton.amqp.security.SaslInit;
import org.apache.qpid.proton.amqp.transport.FrameBody;
import org.apache.qpid.proton.amqp.transport.Open;
import org.apache.qpid.proton.codec.AMQPDefinedTypes;
import org.apache.qpid.proton.codec.ByteBufferDecoder;
import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.codec.DecoderImpl;
import org.apache.qpid.proton.codec.EncoderImpl;
import org.apache.qpid.proton.engine.TransportException;
import org.junit.Test;

/**
 * TODO test case where header is malformed
 * TODO test case where input provides frame and half etc
 */
public class SaslFrameParserTest
{
    private final SaslFrameHandler _mockSaslFrameHandler = mock(SaslFrameHandler.class);
    private final ByteBufferDecoder _mockDecoder = mock(ByteBufferDecoder.class);
    private final SaslFrameParser _frameParser;
    private final SaslFrameParser _frameParserWithMockDecoder = new SaslFrameParser(_mockSaslFrameHandler, _mockDecoder);
    private final AmqpFramer _amqpFramer = new AmqpFramer();

    private final SaslInit _saslFrameBody;
    private final ByteBuffer _saslFrameBytes;

    public SaslFrameParserTest()
    {
        DecoderImpl decoder = new DecoderImpl();
        EncoderImpl encoder = new EncoderImpl(decoder);
        AMQPDefinedTypes.registerAllTypes(decoder,encoder);

        _frameParser = new SaslFrameParser(_mockSaslFrameHandler, decoder);
        _saslFrameBody = new SaslInit();
        _saslFrameBody.setMechanism(Symbol.getSymbol("unused"));
        _saslFrameBytes = ByteBuffer.wrap(_amqpFramer.generateSaslFrame(0, new byte[0], _saslFrameBody));
    }

    @Test
    public void testInputOfValidFrame()
    {
        sendAmqpSaslHeader(_frameParser);

        when(_mockSaslFrameHandler.isDone()).thenReturn(false);

        _frameParser.input(_saslFrameBytes);

        verify(_mockSaslFrameHandler).handle(isA(SaslInit.class), (Binary)isNull());
    }

    @Test
    public void testInputOfInvalidFrame_causesErrorAndRefusesFurtherInput()
    {
        sendAmqpSaslHeader(_frameParserWithMockDecoder);

        String exceptionMessage = "dummy decode exception";
        when(_mockDecoder.readObject()).thenThrow(new DecodeException(exceptionMessage));

        // We send a valid frame but the mock decoder has been configured to reject it
        try {
            _frameParserWithMockDecoder.input(_saslFrameBytes);
            fail("expected exception");
        } catch (TransportException e) {
            assertThat(e.getMessage(), containsString(exceptionMessage));
        }

        verify(_mockSaslFrameHandler, never()).handle(any(SaslFrameBody.class), any(Binary.class));

        // Check that any further interaction causes an error TransportResult.
        try {
            _frameParserWithMockDecoder.input(ByteBuffer.wrap("".getBytes()));
            fail("expected exception");
        } catch (TransportException e) {
            // this is expected
        }
    }

    @Test
    public void testInputOfNonSaslFrame_causesErrorAndRefusesFurtherInput()
    {
        sendAmqpSaslHeader(_frameParserWithMockDecoder);

        FrameBody nonSaslFrame = new Open();
        when(_mockDecoder.readObject()).thenReturn(nonSaslFrame);

        // We send a valid frame but the mock decoder has been configured to reject it
        try {
            _frameParserWithMockDecoder.input(_saslFrameBytes);
            fail("expected exception");
        } catch (TransportException e) {
            assertThat(e.getMessage(), containsString("Unexpected frame type encountered."));
        }

        verify(_mockSaslFrameHandler, never()).handle(any(SaslFrameBody.class), any(Binary.class));

        // Check that any further interaction causes an error TransportResult.
        try {
            _frameParserWithMockDecoder.input(ByteBuffer.wrap("".getBytes()));
            fail("expected exception");
        } catch (TransportException e) {
            // this is expected
        }
    }

    private void sendAmqpSaslHeader(SaslFrameParser saslFrameParser)
    {
        saslFrameParser.input(ByteBuffer.wrap(AmqpHeader.SASL_HEADER));
    }

}
