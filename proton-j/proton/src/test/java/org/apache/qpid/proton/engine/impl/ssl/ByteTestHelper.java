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
package org.apache.qpid.proton.engine.impl.ssl;

import static org.junit.Assert.assertEquals;

import java.nio.ByteBuffer;
import java.util.Arrays;

import org.junit.Assert;

public class ByteTestHelper
{
    public static void assertArrayUntouchedExcept(String expectedString, byte[] actualBytes)
    {
        assertArrayUntouchedExcept(expectedString, actualBytes, 0);
    }

    public static void assertArrayUntouchedExcept(String expectedString, byte[] actualBytes, int offset)
    {
        byte[] string = expectedString.getBytes();

        byte[] expectedBytes = createFilledBuffer(actualBytes.length);
        System.arraycopy(string, 0, expectedBytes, offset, string.length);

        assertEquals(new String(expectedBytes), new String(actualBytes));
    }

    /**
     * Intended to be used by tests to write into, allowing us to check that only the expected elements got overwritten.
     * @return a byte array of the specified length containing @@@...
     */
    public static byte[] createFilledBuffer(int length)
    {
        byte[] expectedBytes = new byte[length];
        Arrays.fill(expectedBytes, (byte)'@');
        return expectedBytes;
    }

    public static void assertByteEquals(char expectedAsChar, byte actual)
    {
        Assert.assertEquals((byte)expectedAsChar, actual);
    }

    public static void assertByteBufferContents(ByteBuffer byteBuffer, String expectedContents)
    {
        assertByteBufferContents(byteBuffer, expectedContents, null);
    }

    public static void assertByteBufferContents(ByteBuffer byteBuffer, String expectedContents, String message)
    {
        int expectedLimit = expectedContents.length();

        assertEquals("Position of readable byte buffer should initially be zero", 0, byteBuffer.position());
        assertEquals("Limit of readable byte buffer should equal length of expected contents", expectedLimit, byteBuffer.limit());

        byte[] bytes = new byte[expectedLimit];
        byteBuffer.get(bytes);
        if(message != null)
        {
            assertEquals(message, expectedContents, new String(bytes));
        }
        else
        {
            assertEquals("Unexpected contents", expectedContents, new String(bytes));
        }
    }
}
