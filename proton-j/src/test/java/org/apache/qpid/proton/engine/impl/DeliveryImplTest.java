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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import org.junit.Test;
import org.mockito.Mockito;

public class DeliveryImplTest
{
    @Test
    public void testDefaultMessageFormat() throws Exception
    {
        DeliveryImpl delivery = new DeliveryImpl(null, Mockito.mock(LinkImpl.class), null);
        assertEquals("Unexpected value", 0L, DeliveryImpl.DEFAULT_MESSAGE_FORMAT);
        assertEquals("Unexpected message format", DeliveryImpl.DEFAULT_MESSAGE_FORMAT, delivery.getMessageFormat());
    }

    @Test
    public void testSetGetMessageFormat() throws Exception
    {
        DeliveryImpl delivery = new DeliveryImpl(null, Mockito.mock(LinkImpl.class), null);

        // lowest value and default
        long newFormat = 0;
        delivery.setMessageFormat(newFormat);
        assertEquals("Unexpected message format", newFormat, delivery.getMessageFormat());

        newFormat = 123456;
        delivery.setMessageFormat(newFormat);
        assertEquals("Unexpected message format", newFormat, delivery.getMessageFormat());

        // Highest value
        newFormat = (1L << 32) - 1;
        delivery.setMessageFormat(newFormat);
        assertEquals("Unexpected message format", newFormat, delivery.getMessageFormat());
    }

    @Test
    public void testSetMessageFormatWithNegativeNumberThrowsIAE() throws Exception
    {
        DeliveryImpl delivery = new DeliveryImpl(null, Mockito.mock(LinkImpl.class), null);
        try
        {
            delivery.setMessageFormat(-1L);
            fail("Expected exception to be thrown");
        }
        catch(IllegalArgumentException iae)
        {
            //expected
        }
    }

    @Test
    public void testSetMessageFormatWithValueAboveUnsignedIntRangeThrowsIAE() throws Exception
    {
        DeliveryImpl delivery = new DeliveryImpl(null, Mockito.mock(LinkImpl.class), null);
        try
        {
            // 2^32 is first positive value outside allowed range
            delivery.setMessageFormat(1L << 32);
            fail("Expected exception to be thrown");
        }
        catch(IllegalArgumentException iae)
        {
            //expected
        }
    }
}
