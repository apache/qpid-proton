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

import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.codec.UnsignedIntegerType.UnsignedIntegerEncoding;
import org.junit.Test;

public class UnsignedIntegerTypeTest
{
    @Test
    public void testGetEncodingWithZero()
    {
        DecoderImpl decoder = new DecoderImpl();
        EncoderImpl encoder = new EncoderImpl(decoder);
        UnsignedIntegerType ult = new UnsignedIntegerType(encoder, decoder);

        //values of 0 are encoded as a specific type
        UnsignedIntegerEncoding encoding = ult.getEncoding(UnsignedInteger.valueOf(0L));
        assertEquals("incorrect encoding returned", EncodingCodes.UINT0, encoding.getEncodingCode());
    }

    @Test
    public void testGetEncodingWithSmallPositiveValue()
    {
        DecoderImpl decoder = new DecoderImpl();
        EncoderImpl encoder = new EncoderImpl(decoder);
        UnsignedIntegerType ult = new UnsignedIntegerType(encoder, decoder);

        //values between 0 and 255 are encoded as a specific 'small' type using a single byte
        UnsignedIntegerEncoding encoding = ult.getEncoding(UnsignedInteger.valueOf(1L));
        assertEquals("incorrect encoding returned", EncodingCodes.SMALLUINT, encoding.getEncodingCode());
    }

    @Test
    public void testGetEncodingWithTwoToThirtyOne()
    {
        DecoderImpl decoder = new DecoderImpl();
        EncoderImpl encoder = new EncoderImpl(decoder);
        UnsignedIntegerType ult = new UnsignedIntegerType(encoder, decoder);

        long val = Integer.MAX_VALUE + 1L;
        UnsignedIntegerEncoding encoding = ult.getEncoding(UnsignedInteger.valueOf(val));
        assertEquals("incorrect encoding returned", EncodingCodes.UINT, encoding.getEncodingCode());
    }
}
