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
 *
 */
package org.apache.qpid.proton.engine.impl;

import static org.apache.qpid.proton.engine.impl.AmqpHeader.HEADER;
import static org.apache.qpid.proton.engine.impl.TransportTestHelper.stringOfLength;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import java.nio.ByteBuffer;

import org.apache.qpid.proton.amqp.transport.Begin;
import org.apache.qpid.proton.amqp.transport.Open;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.engine.TransportException;
import org.apache.qpid.proton.framing.TransportFrame;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;

public class TransportImplTest
{
    @SuppressWarnings("deprecation")
    private TransportImpl _transport = new TransportImpl();

    private static final int CHANNEL_ID = 1;
    private static final TransportFrame TRANSPORT_FRAME_BEGIN = new TransportFrame(CHANNEL_ID, new Begin(), null);
    private static final TransportFrame TRANSPORT_FRAME_OPEN = new TransportFrame(CHANNEL_ID, new Open(), null);

    @Rule
    public ExpectedException _expectedException = ExpectedException.none();

    @Test
    public void testInput()
    {
        ByteBuffer buffer = _transport.getInputBuffer();
        buffer.put(HEADER);
        _transport.processInput().checkIsOk();

        assertNotNull(_transport.getInputBuffer());
    }

    @Test
    public void testInitialProcessIsNoop()
    {
        _transport.process();
    }

    @Test
    public void testProcessIsIdempotent()
    {
        _transport.process();
        _transport.process();
    }

    /**
     * Empty input is always allowed by {@link Transport#getInputBuffer()} and
     * {@link Transport#processInput()}, in contrast to the old API.
     *
     * @see TransportImplTest#testEmptyInputBeforeBindUsingOldApi_causesTransportException()
     */
    @Test
    public void testEmptyInput_isAllowed()
    {
        _transport.getInputBuffer();
        _transport.processInput().checkIsOk();
    }

    /**
     * Tests the end-of-stream behaviour specified by {@link Transport#input(byte[], int, int)}.
     */
    @Test
    public void testEmptyInputBeforeBindUsingOldApi_causesTransportException()
    {
        _expectedException.expect(TransportException.class);
        _expectedException.expectMessage("Unexpected EOS when remote connection not closed: connection aborted");
        _transport.input(new byte [0], 0, 0);
    }

    /**
     * TODO it's not clear why empty input is specifically allowed in this case.
     */
    @Test
    public void testEmptyInputWhenRemoteConnectionIsClosedUsingOldApi_isAllowed()
    {
        @SuppressWarnings("deprecation")
        ConnectionImpl connection = new ConnectionImpl();
        _transport.bind(connection);
        connection.setRemoteState(EndpointState.CLOSED);
        _transport.input(new byte [0], 0, 0);
    }

    @Test
    public void testOutupt()
    {
        {
            // TransportImpl's underlying output spontaneously outputs the AMQP header
            final ByteBuffer outputBuffer = _transport.getOutputBuffer();
            assertEquals(HEADER.length, outputBuffer.remaining());

            byte[] outputBytes = new byte[HEADER.length];
            outputBuffer.get(outputBytes);
            assertArrayEquals(HEADER, outputBytes);

            _transport.outputConsumed();
        }

        {
            final ByteBuffer outputBuffer = _transport.getOutputBuffer();
            assertEquals(0, outputBuffer.remaining());
            _transport.outputConsumed();
        }
    }

    @Test
    public void testTransportInitiallyHandlesFrames()
    {
        assertTrue(_transport.isHandlingFrames());
    }

    @Test
    public void testBoundTransport_continuesToHandleFrames()
    {
        @SuppressWarnings("deprecation")
        Connection connection = new ConnectionImpl();

        assertTrue(_transport.isHandlingFrames());

        _transport.bind(connection);

        assertTrue(_transport.isHandlingFrames());

        _transport.handleFrame(TRANSPORT_FRAME_OPEN);

        assertTrue(_transport.isHandlingFrames());
    }

    @Test
    public void testUnboundTransport_stopsHandlingFrames()
    {
        assertTrue(_transport.isHandlingFrames());

        _transport.handleFrame(TRANSPORT_FRAME_OPEN);

        assertFalse(_transport.isHandlingFrames());
    }

    @Test
    public void testHandleFrameWhenNotHandling_throwsIllegalStateException()
    {
        assertTrue(_transport.isHandlingFrames());

        _transport.handleFrame(TRANSPORT_FRAME_OPEN);

        assertFalse(_transport.isHandlingFrames());

        _expectedException.expect(IllegalStateException.class);
        _transport.handleFrame(TRANSPORT_FRAME_BEGIN);
    }

    @Test
    public void testOutputTooBigToBeWrittenInOneGo()
    {
        int smallMaxFrameSize = 512;
        _transport = new TransportImpl(smallMaxFrameSize);

        @SuppressWarnings("deprecation")
        Connection conn = new ConnectionImpl();
        _transport.bind(conn);

        // Open frame sized in order to produce a frame that will almost fill output buffer
        conn.setHostname(stringOfLength("x", 500));
        conn.open();

        // Close the connection to generate a Close frame which will cause an overflow
        // internally - we'll get the remaining bytes on the next interaction.
        conn.close();

        ByteBuffer buf = _transport.getOutputBuffer();
        assertEquals("Expecting buffer to be full", smallMaxFrameSize, buf.remaining());
        buf.position(buf.limit());
        _transport.outputConsumed();

        buf  = _transport.getOutputBuffer();
        assertTrue("Expecting second buffer to have bytes", buf.remaining() > 0);
        assertTrue("Expecting second buffer to not be full", buf.remaining() < Transport.MIN_MAX_FRAME_SIZE);
    }

    @Test
    public void testAttemptToInitiateSaslAfterProcessingBeginsCausesIllegalStateException()
    {
        _transport.process();

        try
        {
            _transport.sasl();
        }
        catch(IllegalStateException ise)
        {
            //expected, sasl must be initiated before processing begins
        }
    }
}
