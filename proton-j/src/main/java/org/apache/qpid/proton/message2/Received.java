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

import java.util.List;

import org.apache.qpid.proton.codec2.DecodeException;
import org.apache.qpid.proton.codec2.DescribedTypeFactory;
import org.apache.qpid.proton.codec2.Encodable;
import org.apache.qpid.proton.codec2.Encoder;
import org.apache.qpid.proton.transport2.DeliveryState;

public final class Received implements DeliveryState, Encodable
{
    public final static long DESCRIPTOR_LONG = 0x0000000000000023L;

    public final static String DESCRIPTOR_STRING = "amqp:received:list";

    public final static Factory FACTORY = new Factory();

    private int _sectionNumber;

    private long _sectionOffset;

    public int getSectionNumber()
    {
        return _sectionNumber;
    }

    public void setSectionNumber(int sectionNumber)
    {
        _sectionNumber = sectionNumber;
    }

    public long getSectionOffset()
    {
        return _sectionOffset;
    }

    public void setSectionOffset(long sectionOffset)
    {
        _sectionOffset = sectionOffset;
    }

    @Override
    public void encode(Encoder encoder)
    {
        encoder.putDescriptor();
        encoder.putUlong(DESCRIPTOR_LONG);
        encoder.putList();
        encoder.putUint(_sectionNumber);
        encoder.putUlong(_sectionOffset);
        encoder.end();
    }

    public static final class Factory implements DescribedTypeFactory
    {
        @SuppressWarnings("unchecked")
        public Object create(Object in) throws DecodeException
        {
            List<Object> l = (List<Object>) in;
            Received received = new Received();

            switch (2 - l.size())
            {

            case 0:
                received.setSectionOffset((Long) l.get(1));
            case 1:
                received.setSectionNumber((Integer) l.get(0));
            }
            return received;
        }
    }

    @Override
    public String toString()
    {
        return "Received{" + "sectionNumber=" + _sectionNumber + ", sectionOffset=" + _sectionOffset + '}';
    }
}
