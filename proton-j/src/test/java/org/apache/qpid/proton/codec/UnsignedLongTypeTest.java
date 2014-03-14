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
package org.apache.qpid.proton.codec;

import static org.junit.Assert.*;

import java.math.BigInteger;

import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.codec.UnsignedLongType.UnsignedLongEncoding;
import org.junit.Test;

public class UnsignedLongTypeTest
{
    @Test
    public void testGetEncodingWithZero()
    {
        DecoderImpl decoder = new DecoderImpl();
        EncoderImpl encoder = new EncoderImpl(decoder);
        UnsignedLongType ult = new UnsignedLongType(encoder, decoder);

        //values of 0 are encoded as a specific type
        UnsignedLongEncoding encoding = ult.getEncoding(UnsignedLong.valueOf(0L));
        assertEquals("incorrect encoding returned", EncodingCodes.ULONG0, encoding.getEncodingCode());
    }

    @Test
    public void testGetEncodingWithSmallPositiveValue()
    {
        DecoderImpl decoder = new DecoderImpl();
        EncoderImpl encoder = new EncoderImpl(decoder);
        UnsignedLongType ult = new UnsignedLongType(encoder, decoder);

        //values between 0 and 255 are encoded as a specific 'small' type using a single byte
        UnsignedLongEncoding encoding = ult.getEncoding(UnsignedLong.valueOf(1L));
        assertEquals("incorrect encoding returned", EncodingCodes.SMALLULONG, encoding.getEncodingCode());
    }

    @Test
    public void testGetEncodingWithTwoToSixtyThree()
    {
        DecoderImpl decoder = new DecoderImpl();
        EncoderImpl encoder = new EncoderImpl(decoder);
        UnsignedLongType ult = new UnsignedLongType(encoder, decoder);

        BigInteger bigInt = BigInteger.valueOf(Long.MAX_VALUE).add(BigInteger.ONE);
        UnsignedLongEncoding encoding = ult.getEncoding(UnsignedLong.valueOf(bigInt));
        assertEquals("incorrect encoding returned", EncodingCodes.ULONG, encoding.getEncodingCode());
    }
}
