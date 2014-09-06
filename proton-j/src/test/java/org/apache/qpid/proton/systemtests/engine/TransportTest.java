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
package org.apache.qpid.proton.systemtests.engine;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;

import org.apache.qpid.proton.Proton;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.engine.TransportException;
import org.junit.Ignore;
import org.junit.Test;

/**
 * TODO add test for 2.3.1 "The frame is malformed if the size is less than the size of the frame header (8 bytes)"
 * TODO add test using empty byte arrays (calling {@link Transport#input(byte[], int, int)} with empty byte array in Proton-j-impl currently throws "TransportException Unexpected EOS")
 */
public class TransportTest
{
    private final Transport _transport = Proton.transport();

    /**
     * Note that Proton does not yet give the application explicit control over protocol version negotiation
     * TODO does Proton give *visibility* of the negotiated protocol version?
     */
    public void testReceiptOfHeaderContainingUnsupportedProtocolVersionNumber_causesAmqp10Response()
    {
    }

    @Test
    @Ignore("Reinstate once it is agreed how error condition will be reported by to use of API")
    public void testReceiptOfNonAmqpHeader_causesAmqp10Response()
    {
        byte[] nonAmqpHeader = "HTTP/1.0".getBytes();
        try
        {
            _transport.input(nonAmqpHeader, 0, nonAmqpHeader.length);

            // TODO Proton-c gives  rv PN_ERROR and a pn_transport_error "AMQP header mismatch: 'HTTP/1.0'" and then
            // jni layer turns this into a TransportException.
            // Proton-j just throws TransportException
        }
        catch (TransportException te)
        {
            // TODO - exception should not be thrown
        }

        byte[] buf = new byte[255];
        int bytesWritten = _transport.output(buf, 0, buf.length);
        byte[] response = new byte[bytesWritten];
        System.arraycopy(buf, 0, response, 0, bytesWritten);
        assertArrayEquals("AMQP\0\1\0\0".getBytes(), response);

        // how should further input be handled??

        assertTransportRefusesFurtherInputOutput(_transport);
    }

    private void assertTransportRefusesFurtherInputOutput(Transport transport)
    {
        byte[] sourceBufferThatShouldBeUnread = "REFUSEME".getBytes();
        int bytesConsumed = transport.input(sourceBufferThatShouldBeUnread, 0, sourceBufferThatShouldBeUnread.length);
        // assertEquals(-1, bytesConsumed); // TODO reinstate with testReceiptOfNonAmqpHeader_causesAmqp10Response

        byte[] destBufferThatShouldRemainUnwritten = new byte[255];
        int bytesWritten = transport.output(destBufferThatShouldRemainUnwritten, 0, destBufferThatShouldRemainUnwritten.length);
        assertEquals(-1, bytesWritten);
    }
}
