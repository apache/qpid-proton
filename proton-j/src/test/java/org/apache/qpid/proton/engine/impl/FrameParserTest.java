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

import static org.apache.qpid.proton.engine.Transport.DEFAULT_MAX_FRAME_SIZE;
import static org.apache.qpid.proton.engine.impl.AmqpHeader.HEADER;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.fail;
import static org.junit.matchers.JUnitMatchers.containsString;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.argThat;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.nio.ByteBuffer;

import org.apache.qpid.proton.amqp.transport.Close;
import org.apache.qpid.proton.amqp.transport.FrameBody;
import org.apache.qpid.proton.amqp.transport.Open;
import org.apache.qpid.proton.codec.AMQPDefinedTypes;
import org.apache.qpid.proton.codec.DecoderImpl;
import org.apache.qpid.proton.codec.EncoderImpl;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.engine.TransportException;
import org.apache.qpid.proton.engine.TransportResult;
import org.apache.qpid.proton.engine.TransportResult.Status;
import org.apache.qpid.proton.framing.TransportFrame;
import org.hamcrest.Description;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.mockito.ArgumentMatcher;
import org.mockito.InOrder;

// TODO test a frame with a payload (potentially followed by another frame)
public class FrameParserTest
{
    private FrameHandler _mockFrameHandler = mock(FrameHandler.class);
    private DecoderImpl _decoder = new DecoderImpl();
    private EncoderImpl _encoder = new EncoderImpl(_decoder);
    private final FrameParser _frameParser = new FrameParser(_mockFrameHandler, _decoder, DEFAULT_MAX_FRAME_SIZE);

    private final AmqpFramer _amqpFramer = new AmqpFramer();

    @Before
    public void setUp()
    {
        AMQPDefinedTypes.registerAllTypes(_decoder, _encoder);

        when(_mockFrameHandler.isHandlingFrames()).thenReturn(true);
    }

    @Test
    public void testInputOfInvalidProtocolHeader_causesErrorAndRefusesFurtherInput()
    {
        String headerMismatchMessage = "AMQP header mismatch";
        ByteBuffer buffer = _frameParser.tail();
        buffer.put("hello".getBytes());
        _frameParser.process();
        assertEquals(_frameParser.capacity(), Transport.END_OF_STREAM);
    }

    @Test
    public void testInputOfValidProtocolHeader()
    {
        ByteBuffer buffer = _frameParser.tail();
        buffer.put(HEADER);
        _frameParser.process();

        assertNotNull(_frameParser.tail());
    }

    @Test
    public void testInputOfValidProtocolHeaderInMultipleChunks()
    {
        {
            ByteBuffer buffer = _frameParser.tail();
            buffer.put(HEADER, 0, 2);
            _frameParser.process();
        }

        {
            ByteBuffer buffer = _frameParser.tail();
            buffer.put(HEADER, 2, HEADER.length - 2);
            _frameParser.process();
        }

        assertNotNull(_frameParser.tail());
    }

    @Test
    public void testInputOfValidFrame_invokesFrameTransportCallback()
    {
        sendHeader();

        // now send an open frame
        ByteBuffer buffer = _frameParser.tail();

        Open openFrame = generateOpenFrame();
        int channel = 0;
        byte[] frame = _amqpFramer.generateFrame(channel, openFrame);
        buffer.put(frame);

        _frameParser.process();
        verify(_mockFrameHandler).handleFrame(frameMatching(channel, openFrame));
    }

    @Test
    public void testInputOfFrameInMultipleChunks_invokesFrameTransportCallback()
    {
        sendHeader();

        Open openFrame = generateOpenFrame();
        int channel = 0;
        byte[] frame = _amqpFramer.generateFrame(channel, openFrame);
        int lengthOfFirstChunk = 2;
        int lengthOfSecondChunk = (frame.length - lengthOfFirstChunk)/2;
        int lengthOfThirdChunk = frame.length - lengthOfFirstChunk - lengthOfSecondChunk;

        // send the first chunk
        {
            ByteBuffer buffer = _frameParser.tail();

            buffer.put(frame, 0, lengthOfFirstChunk);

            _frameParser.process();

            verify(_mockFrameHandler, never()).handleFrame(any(TransportFrame.class));
        }

        // send the second chunk
        {
            ByteBuffer buffer = _frameParser.tail();

            int secondChunkOffset = lengthOfFirstChunk;
            buffer.put(frame, secondChunkOffset, lengthOfSecondChunk);

            _frameParser.process();
            verify(_mockFrameHandler, never()).handleFrame(any(TransportFrame.class));
        }

        // send the third and final chunk
        {
            ByteBuffer buffer = _frameParser.tail();

            int thirdChunkOffset = lengthOfFirstChunk + lengthOfSecondChunk;
            buffer.put(frame, thirdChunkOffset, lengthOfThirdChunk);

            _frameParser.process();
            verify(_mockFrameHandler).handleFrame(frameMatching(channel, openFrame));
        }
    }

    @Test
    public void testInputOfTwoFrames_invokesFrameTransportTwice()
    {
        sendHeader();

        int channel = 0;
        Open openFrame = generateOpenFrame();
        byte[] openFrameBytes = _amqpFramer.generateFrame(channel, openFrame);

        Close closeFrame = generateCloseFrame();
        byte[] closeFrameBytes = _amqpFramer.generateFrame(channel, closeFrame);

        _frameParser.tail()
            .put(openFrameBytes)
            .put(closeFrameBytes);

        _frameParser.process();

        InOrder inOrder = inOrder(_mockFrameHandler);
        inOrder.verify(_mockFrameHandler).handleFrame(frameMatching(channel, openFrame));
        inOrder.verify(_mockFrameHandler).handleFrame(frameMatching(channel, closeFrame));
    }

    @Test
    public void testFrameTransportTemporarilyRefusesOpenFrame()
    {
        when(_mockFrameHandler.isHandlingFrames()).thenReturn(false);

        sendHeader();

        // now send an open frame
        int channel = 0;
        Open openFrame = generateOpenFrame();
        {
            ByteBuffer buffer = _frameParser.tail();

            byte[] frame = _amqpFramer.generateFrame(channel, openFrame);
            buffer.put(frame);

            _frameParser.process();
        }

        verify(_mockFrameHandler, never()).handleFrame(any(TransportFrame.class));

        when(_mockFrameHandler.isHandlingFrames()).thenReturn(true);

        // now ensure that the held frame gets sent on second input
        Close closeFrame = generateCloseFrame();
        {
            ByteBuffer buffer = _frameParser.tail();

            byte[] frame = _amqpFramer.generateFrame(channel, closeFrame);
            buffer.put(frame);

            _frameParser.process();
        }

        InOrder inOrder = inOrder(_mockFrameHandler);
        inOrder.verify(_mockFrameHandler).handleFrame(frameMatching(channel, openFrame));
        inOrder.verify(_mockFrameHandler).handleFrame(frameMatching(channel, closeFrame));
    }

    @Test
    public void testFrameTransportTemporarilyRefusesOpenAndCloseFrame()
    {
        when(_mockFrameHandler.isHandlingFrames()).thenReturn(false);

        sendHeader();

        // now send an open frame
        int channel = 0;
        Open openFrame = generateOpenFrame();
        {
            ByteBuffer buffer = _frameParser.tail();

            byte[] frame = _amqpFramer.generateFrame(channel, openFrame);
            buffer.put(frame);

            _frameParser.process();
        }
        verify(_mockFrameHandler, never()).handleFrame(any(TransportFrame.class));

        // now send a close frame
        Close closeFrame = generateCloseFrame();
        {
            ByteBuffer buffer = _frameParser.tail();

            byte[] frame = _amqpFramer.generateFrame(channel, closeFrame);
            buffer.put(frame);

            _frameParser.process();
        }
        verify(_mockFrameHandler, never()).handleFrame(any(TransportFrame.class));

        when(_mockFrameHandler.isHandlingFrames()).thenReturn(true);

        _frameParser.flush();

        InOrder inOrder = inOrder(_mockFrameHandler);
        inOrder.verify(_mockFrameHandler).handleFrame(frameMatching(channel, openFrame));
        inOrder.verify(_mockFrameHandler).handleFrame(frameMatching(channel, closeFrame));
    }

    private void sendHeader() throws TransportException
    {
        ByteBuffer buffer = _frameParser.tail();
        buffer.put(HEADER);
        _frameParser.process();
    }

    private Open generateOpenFrame()
    {
        Open open = new Open();
        open.setContainerId("containerid");
        return open;
    }

    private Close generateCloseFrame()
    {
        Close close = new Close();
        return close;
    }

    private TransportFrame frameMatching(int channel, FrameBody frameBody)
    {
        return argThat(new TransportFrameMatcher(channel, frameBody));
    }

    private class TransportFrameMatcher extends ArgumentMatcher<TransportFrame>
    {
        private final TransportFrame _expectedTransportFrame;

        TransportFrameMatcher(int expectedChannel, FrameBody expectedFrameBody)
        {
            _expectedTransportFrame = new TransportFrame(expectedChannel, expectedFrameBody, null);
        }

        @Override
        public boolean matches(Object transportFrameObj)
        {
            if(transportFrameObj == null)
            {
                return false;
            }

            TransportFrame transportFrame = (TransportFrame)transportFrameObj;
            FrameBody actualFrame = transportFrame.getBody();

            int _expectedChannel = _expectedTransportFrame.getChannel();
            FrameBody expectedFrame = _expectedTransportFrame.getBody();

            return _expectedChannel == transportFrame.getChannel()
                    && expectedFrame.getClass().equals(actualFrame.getClass());
        }

        @Override
        public void describeTo(Description description)
        {
            super.describeTo(description);
            description.appendText("Expected: " + _expectedTransportFrame);
        }
    }
}
