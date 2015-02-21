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

public final class Header implements Encodable
{
    public final static long DESCRIPTOR_LONG = 0x0000000000000070L;

    public final static String DESCRIPTOR_STRING = "amqp:header:list";

    private boolean _durable;

    private byte _priority;

    private int _ttl;

    private boolean _firstAcquirer;

    private int _deliveryCount;

    public Boolean getDurable()
    {
        return _durable;
    }

    public void setDurable(Boolean durable)
    {
        _durable = durable;
    }

    public byte getPriority()
    {
        return _priority;
    }

    public void setPriority(byte priority)
    {
        _priority = priority;
    }

    public int getTtl()
    {
        return _ttl;
    }

    public void setTtl(int ttl)
    {
        _ttl = ttl;
    }

    public Boolean getFirstAcquirer()
    {
        return _firstAcquirer;
    }

    public void setFirstAcquirer(Boolean firstAcquirer)
    {
        _firstAcquirer = firstAcquirer;
    }

    public int getDeliveryCount()
    {
        return _deliveryCount;
    }

    public void setDeliveryCount(int deliveryCount)
    {
        _deliveryCount = deliveryCount;
    }

    @Override
    public void encode(Encoder encoder)
    {
        encoder.putDescriptor();
        encoder.putUlong(DESCRIPTOR_LONG);
        encoder.putList();
        encoder.putBoolean(_durable);
        encoder.putUbyte(_priority);
        encoder.putUint(_ttl);
        encoder.putBoolean(_firstAcquirer);
        encoder.putUint(_deliveryCount);
        encoder.end();
    }

    public static final class Factory implements DescribedTypeFactory
    {
        @SuppressWarnings("unchecked")
        public Object create(Object in) throws DecodeException
        {
            List<Object> l = (List<Object>) in;
            Header header = new Header();

            switch (5 - l.size())
            {

            case 0:
                header.setDeliveryCount((Integer) l.get(4));
            case 1:
                header.setFirstAcquirer(l.get(3) == null ? false : (Boolean) l.get(3));
            case 2:
                header.setTtl((Integer) l.get(2));
            case 3:
                header.setPriority(l.get(1) == null ? 4 : (Byte) l.get(1));
            case 4:
                header.setDurable(l.get(0) == null ? false : (Boolean) l.get(0));
            }
            return header;
        }
    }

    @Override
    public String toString()
    {
        return "Header{" +
               "durable=" + _durable +
               ", priority=" + _priority +
               ", ttl=" + _ttl +
               ", firstAcquirer=" + _firstAcquirer +
               ", deliveryCount=" + _deliveryCount +
               '}';
    }
}