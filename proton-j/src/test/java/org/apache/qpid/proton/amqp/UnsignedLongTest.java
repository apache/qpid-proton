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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import java.math.BigInteger;

import org.junit.Test;

public class UnsignedLongTest
{
    private static final byte[] TWO_TO_64_PLUS_ONE_BYTES = new byte[] { (byte) 1, (byte) 0, (byte) 0, (byte) 0, (byte) 0, (byte) 0, (byte) 0, (byte) 0, (byte) 1 };
    private static final byte[] TWO_TO_64_MINUS_ONE_BYTES = new byte[] {(byte) 1, (byte) 1, (byte) 1, (byte) 1, (byte) 1, (byte) 1, (byte) 1, (byte) 1 };

    @Test
    public void testValueOfStringWithNegativeNumberThrowsNFE() throws Exception
    {
        try
        {
            UnsignedLong.valueOf("-1");
            fail("Expected exception was not thrown");
        }
        catch(NumberFormatException nfe)
        {
            //expected
        }
    }

    @Test
    public void testValueOfBigIntegerWithNegativeNumberThrowsNFE() throws Exception
    {
        try
        {
            UnsignedLong.valueOf(BigInteger.valueOf(-1L));
            fail("Expected exception was not thrown");
        }
        catch(NumberFormatException nfe)
        {
            //expected
        }
    }

    @Test
    public void testValuesOfStringWithinRangeSucceed() throws Exception
    {
        //check 0 (min) to confirm success
        UnsignedLong min = UnsignedLong.valueOf("0");
        assertEquals("unexpected value", 0, min.longValue());

        //check 2^64 -1 (max) to confirm success
        BigInteger onLimit = new BigInteger(TWO_TO_64_MINUS_ONE_BYTES);
        String onlimitString = onLimit.toString();
        UnsignedLong max =  UnsignedLong.valueOf(onlimitString);
        assertEquals("unexpected value", onLimit, max.bigIntegerValue());
    }

    @Test
    public void testValuesOfBigIntegerWithinRangeSucceed() throws Exception
    {
        //check 0 (min) to confirm success
        UnsignedLong min = UnsignedLong.valueOf(BigInteger.ZERO);
        assertEquals("unexpected value", 0, min.longValue());

        //check 2^64 -1 (max) to confirm success
        BigInteger onLimit = new BigInteger(TWO_TO_64_MINUS_ONE_BYTES);
        UnsignedLong max =  UnsignedLong.valueOf(onLimit);
        assertEquals("unexpected value", onLimit, max.bigIntegerValue());

        //check Long.MAX_VALUE to confirm success
        UnsignedLong longMax =  UnsignedLong.valueOf(BigInteger.valueOf(Long.MAX_VALUE));
        assertEquals("unexpected value", Long.MAX_VALUE, longMax.longValue());
    }

    @Test
    public void testValueOfStringAboveMaxValueThrowsNFE() throws Exception
    {
        //2^64 + 1 (value 2 over max)
        BigInteger aboveLimit = new BigInteger(TWO_TO_64_PLUS_ONE_BYTES);
        try
        {
            UnsignedLong.valueOf(aboveLimit.toString());
            fail("Expected exception was not thrown");
        }
        catch(NumberFormatException nfe)
        {
            //expected
        }

        //2^64 (value 1 over max)
        aboveLimit = aboveLimit.subtract(BigInteger.ONE);
        try
        {
            UnsignedLong.valueOf(aboveLimit.toString());
            fail("Expected exception was not thrown");
        }
        catch(NumberFormatException nfe)
        {
            //expected
        }
    }

    @Test
    public void testValueOfBigIntegerAboveMaxValueThrowsNFE() throws Exception
    {
        //2^64 + 1 (value 2 over max)
        BigInteger aboveLimit = new BigInteger(TWO_TO_64_PLUS_ONE_BYTES);
        try
        {
            UnsignedLong.valueOf(aboveLimit);
            fail("Expected exception was not thrown");
        }
        catch(NumberFormatException nfe)
        {
            //expected
        }

        //2^64 (value 1 over max)
        aboveLimit = aboveLimit.subtract(BigInteger.ONE);
        try
        {
            UnsignedLong.valueOf(aboveLimit);
            fail("Expected exception was not thrown");
        }
        catch(NumberFormatException nfe)
        {
            //expected
        }
    }
}
