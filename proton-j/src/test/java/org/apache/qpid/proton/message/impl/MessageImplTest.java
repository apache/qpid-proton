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
package org.apache.qpid.proton.message.impl;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;

import java.nio.ByteBuffer;

import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.messaging.Data;
import org.apache.qpid.proton.message.Message;
import org.junit.Test;

public class MessageImplTest
{
    private static final long DATA_SECTION_ULONG_DESCRIPTOR = 0x0000000000000075L;

    @Test
    public void testEncodeOfMessageWithSmallDataBodyOnly()
    {
        doMessageEncodingWithDataBodySectionTestImpl(5);
    }

    @Test
    public void testEncodeOfMessageWithLargerDataBodyOnly()
    {
        doMessageEncodingWithDataBodySectionTestImpl(1024);
    }

    void doMessageEncodingWithDataBodySectionTestImpl(int bytesLength)
    {
        byte[] bytes = generateByteArray(bytesLength);

        byte[] expectedBytes = generateExpectedDataSectionBytes(bytes);
        byte[] encodedBytes = new byte[expectedBytes.length];

        Message msg = Message.Factory.create();
        msg.setBody(new Data(new Binary(bytes)));

        int encodedLength = msg.encode(encodedBytes, 0, encodedBytes.length);

        assertArrayEquals("Encoded bytes do not match expectation", expectedBytes, encodedBytes);
        assertEquals("Encoded length different than expected length", encodedLength, encodedBytes.length);
    }

    private byte[] generateByteArray(int bytesLength)
    {
        byte[] bytes = new byte[bytesLength];
        for(int i = 0; i < bytesLength; i++)
        {
            bytes [i] = (byte) (i % 10);
        }

        return bytes;
    }

    /*
     * http://docs.oasis-open.org/amqp/core/v1.0/os/amqp-core-messaging-v1.0-os.html#type-data
     * http://docs.oasis-open.org/amqp/core/v1.0/os/amqp-core-types-v1.0-os.html
     *
     * ulong encodings:
     *  <encoding code="0x80" category="fixed" width="8" label="64-bit unsigned integer in network byte order"/>
     *  <encoding name="smallulong" code="0x53" category="fixed" width="1" label="unsigned long value in the range 0 to 255 inclusive"/>
     *  <encoding name="ulong0" code="0x44" category="fixed" width="0" label="the ulong value 0"/>
     *
     * binary encodings:
     *  <encoding name="vbin8" code="0xa0" category="variable" width="1" label="up to 2^8 - 1 octets of binary data"/>
     *  <encoding name="vbin32" code="0xb0" category="variable" width="4" label="up to 2^32 - 1 octets of binary data"/>
     */
    byte[] generateExpectedDataSectionBytes(final byte[] payloadBytes)
    {
        int dataBytesLength = 1;         // 0x00 for described-type constructor start
        dataBytesLength += 1;            // smallulong encoding format for data section descriptor
        dataBytesLength += 1;            // smallulong 8bit value
        dataBytesLength += 1;            // vbin variable-width binary encoding format.
        if (payloadBytes.length > 255)
        {
            dataBytesLength += 4;        // 32bit length field.
        }
        else
        {
            dataBytesLength += 1;        // 8bit length field.
        }
        dataBytesLength += payloadBytes.length; // section payload length.

        ByteBuffer buffer = ByteBuffer.allocate(dataBytesLength);

        buffer.put((byte) 0x00);                    // 0x00 for described-type constructor start
        buffer.put((byte) 0x53);                    // smallulong encoding format for data section descriptor
        buffer.put((byte) DATA_SECTION_ULONG_DESCRIPTOR); // smallulong 8bit value
        if (payloadBytes.length > 255)
        {
            buffer.put((byte) 0xb0);                // vbin32 variable-width binary encoding format.
            buffer.putInt(payloadBytes.length);     // 32bit length field.
        }
        else
        {
            buffer.put((byte) 0xa0);                // vbin8 variable-width binary encoding format.
            buffer.put((byte) payloadBytes.length); // 8bit length field.
        }
        buffer.put(payloadBytes);                   // The actual content of given length.

        assertEquals("Unexpected buffer position", dataBytesLength, buffer.position());

        return buffer.array();
    }

}
