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

import static org.junit.Assert.*;

import org.apache.qpid.proton.amqp.Binary;
import org.junit.Test;

public class StringUtilsTest
{
    @Test
    public void testNullBinary()
    {
        assertEquals("unexpected result", "\"\"", StringUtils.toQuotedString(null, 10, true));
    }

    @Test
    public void testEmptyBinaryEmptyArray()
    {
        Binary bin = new Binary(new byte[0]);
        assertEquals("unexpected result", "\"\"", StringUtils.toQuotedString(bin, 10, true));
    }

    @Test
    public void testEmptyBinaryNonEmptyArray()
    {
        byte[] bytes = new byte[] {(byte) 0, (byte) 0, (byte) 0};
        Binary bin = new Binary(bytes, 0, 0);
        assertEquals("unexpected result", "\"\"", StringUtils.toQuotedString(bin, 10, true));
    }

    @Test
    public void testEmptyBinaryNonEmptyArrayWithOffset()
    {
        byte[] bytes = new byte[] {(byte) 0, (byte) 0, (byte) 0};
        Binary bin = new Binary(bytes, 1, 0);
        assertEquals("unexpected result", "\"\"", StringUtils.toQuotedString(bin, 10, true));
    }

    @Test
    public void testBinaryStringifiedSmallerThanGivenMaxLength()
    {
        byte[] bytes = new byte[] {(byte) 0, (byte) 1, (byte) 3, (byte) 65, (byte) 152};
        String expected = "\"\\x00\\x01\\x03A\\x98\"";
        Binary bin = new Binary(bytes);
        assertEquals("unexpected result", expected, StringUtils.toQuotedString(bin, 100, true));
        assertEquals("unexpected result", expected, StringUtils.toQuotedString(bin, 100, false));
    }

    @Test
    public void testBinaryStringifiedSmallerThanGivenMaxLengthWithOffset()
    {
        byte[] bytes = new byte[] {(byte) 0, (byte) 1, (byte) 3, (byte) 65, (byte) 152};
        String expected = "\"\\x01\\x03A\\x98\"";
        Binary bin = new Binary(bytes, 1, 4);
        assertEquals("unexpected result", expected, StringUtils.toQuotedString(bin, 100, true));
        assertEquals("unexpected result", expected, StringUtils.toQuotedString(bin, 100, false));
    }

    @Test
    public void testBinaryStringifiedEqualToGivenMaxLength()
    {
        byte[] bytes = new byte[] {(byte) 0, (byte) 1, (byte) 3, (byte) 65};
        String expected = "\"\\x00\\x01\\x03A\"";
        Binary bin = new Binary(bytes);
        assertEquals("unexpected result", expected, StringUtils.toQuotedString(bin, 15, true));
        assertEquals("unexpected result", expected, StringUtils.toQuotedString(bin, 15, false));

        bytes = new byte[] {(byte) 0, (byte) 1, (byte) 65, (byte) 3};
        expected = "\"\\x00\\x01A\\x03\"";
        bin = new Binary(bytes);
        assertEquals("unexpected result", expected, StringUtils.toQuotedString(bin, 15, true));
        assertEquals("unexpected result", expected, StringUtils.toQuotedString(bin, 15, false));
    }

    @Test
    public void testBinaryStringifiedEqualToGivenMaxLengthWithOffset()
    {
        byte[] bytes = new byte[] {(byte) 0, (byte) 1, (byte) 3, (byte) 65};
        String expected = "\"\\x01\\x03A\"";
        Binary bin = new Binary(bytes, 1, 3);
        assertEquals("unexpected result", expected, StringUtils.toQuotedString(bin, 11, true));
        assertEquals("unexpected result", expected, StringUtils.toQuotedString(bin, 11, false));

        bytes = new byte[] {(byte) 0, (byte) 1, (byte) 65, (byte) 3};
        expected = "\"\\x01A\\x03\"";
        bin = new Binary(bytes, 1, 3);
        assertEquals("unexpected result", expected, StringUtils.toQuotedString(bin, 11, true));
        assertEquals("unexpected result", expected, StringUtils.toQuotedString(bin, 11, false));
    }

    @Test
    public void testBinaryStringifiedLargerThanGivenMaxLength()
    {
        byte[] bytes = new byte[] {(byte) 0, (byte) 1, (byte) 3};
        String expected1 = "\"\\x00\\x01\"";
        Binary bin = new Binary(bytes);

        assertEquals("unexpected result", expected1, StringUtils.toQuotedString(bin, 10, false));

        String expected2 = "\"\\x00\\x01\"...(truncated)";
        assertEquals("unexpected result", expected2, StringUtils.toQuotedString(bin, 10, true));
    }

    @Test
    public void testBinaryStringifiedLargerThanGivenMaxLengthWithOffset()
    {
        byte[] bytes = new byte[] {(byte) 0, (byte) 1, (byte) 3};
        String expected1 = "\"\\x00\\x01\"";
        Binary bin = new Binary(bytes);

        assertEquals("unexpected result", expected1, StringUtils.toQuotedString(bin, 10, false));

        String expected2 = "\"\\x00\\x01\"...(truncated)";
        assertEquals("unexpected result", expected2, StringUtils.toQuotedString(bin, 10, true));
    }
}
