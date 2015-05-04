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

package org.apache.qpid.proton.message2;

import org.apache.qpid.proton.codec2.DecodeException;
import org.apache.qpid.proton.codec2.DescribedTypeFactory;
import org.apache.qpid.proton.codec2.Encodable;
import org.apache.qpid.proton.codec2.Encoder;
import org.apache.qpid.proton.transport.DeliveryState;

public final class Released implements DeliveryState, Outcome, Encodable
{
    public final static long DESCRIPTOR_LONG = 0x0000000000000026L;

    public final static String DESCRIPTOR_STRING = "amqp:released:list";

    public final static Factory FACTORY = new Factory();

    private static final Released INSTANCE = new Released();

    @Override
    public void encode(Encoder encoder)
    {
        encoder.putDescriptor();
        encoder.putUlong(DESCRIPTOR_LONG);
        encoder.end();
    }

    public static final class Factory implements DescribedTypeFactory
    {
        public Object create(Object in) throws DecodeException
        {
            return INSTANCE;
        }
    }

    @Override
    public String toString()
    {
        return "Released{}";
    }

    public static Released getInstance()
    {
        return INSTANCE;
    }
}