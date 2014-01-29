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
package org.apache.qpid.proton.systemtests;

import static org.junit.Assert.*;

import org.junit.Test;

public class BinaryFormatterTest
{

    private BinaryFormatter _binaryFormatter = new BinaryFormatter();

    @Test
    public void testSingleCharacter()
    {
        assertEquals("[ A ]", _binaryFormatter.format("A".getBytes()));
    }

    @Test
    public void testSingleSmallNonCharacter()
    {
        byte[] bytes = new byte[] { (byte)0x1 };
        assertEquals("[x01]", _binaryFormatter.format(bytes));
    }

    @Test
    public void testSingleLargeNonCharacter()
    {
        int numberToUse = 0xa2;
        byte byteToUse = (byte)numberToUse;
        byte[] bytes = new byte[] { byteToUse };
        assertEquals("[xa2]", _binaryFormatter.format(bytes));
    }

    @Test
    public void testComplex()
    {
        byte[] binaryData = new byte[4];
        System.arraycopy("ABC".getBytes(), 0, binaryData, 0, 3);
        binaryData[3] = (byte)0xff;

        String formattedString = _binaryFormatter.format(binaryData);
        String expected = "[ A ][ B ][ C ][xff]";
        assertEquals(expected, formattedString);
    }

    public void testFormatSubArray()
    {
        fail("TODO");
    }
}
