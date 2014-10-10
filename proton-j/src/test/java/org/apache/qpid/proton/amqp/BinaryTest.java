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
package org.apache.qpid.proton.amqp;

import static org.junit.Assert.*;

import java.util.Arrays;

import org.junit.Test;

public class BinaryTest
{

    @Test
    public void testNotEqualsWithDifferentTypeObject()
    {
        Binary bin = createSteppedValueBinary(10);

        assertFalse("Objects should not be equal with different type", bin.equals("not-a-Binary"));
    }

    @Test
    public void testEqualsWithItself()
    {
        Binary bin = createSteppedValueBinary(10);

        assertTrue("Object should be equal to itself", bin.equals(bin));
    }

    @Test
    public void testEqualsWithDifferentBinaryOfSameLengthAndContent()
    {
        int length = 10;
        Binary bin1 = createSteppedValueBinary(length);
        Binary bin2 = createSteppedValueBinary(length);

        assertTrue("Objects should be equal", bin1.equals(bin2));
        assertTrue("Objects should be equal", bin2.equals(bin1));
    }

    @Test
    public void testEqualsWithDifferentLengthBinaryOfDifferentBytes()
    {
        int length1 = 10;
        Binary bin1 = createSteppedValueBinary(length1);
        Binary bin2 = createSteppedValueBinary(length1 + 1);

        assertFalse("Objects should not be equal", bin1.equals(bin2));
        assertFalse("Objects should not be equal", bin2.equals(bin1));
    }

    @Test
    public void testEqualsWithDifferentLengthBinaryOfSameByte()
    {
        Binary bin1 = createNewRepeatedValueBinary(10, (byte) 1);
        Binary bin2 = createNewRepeatedValueBinary(123, (byte) 1);

        assertFalse("Objects should not be equal", bin1.equals(bin2));
        assertFalse("Objects should not be equal", bin2.equals(bin1));
    }

    @Test
    public void testEqualsWithDifferentContentBinary()
    {
        int length = 10;
        Binary bin1 = createNewRepeatedValueBinary(length, (byte) 1);

        Binary bin2 = createNewRepeatedValueBinary(length, (byte) 1);
        bin2.getArray()[5] = (byte) 0;

        assertFalse("Objects should not be equal", bin1.equals(bin2));
        assertFalse("Objects should not be equal", bin2.equals(bin1));
    }

    private Binary createSteppedValueBinary(int length) {
        byte[] bytes = new byte[length];
        for (int i = 0; i < length; i++) {
            bytes[i] = (byte) (length - i);
        }

        return new Binary(bytes);
    }

    private Binary createNewRepeatedValueBinary(int length, byte repeatedByte){
        byte[] bytes = new byte[length];
        Arrays.fill(bytes, repeatedByte);

        return new Binary(bytes);
    }
}
