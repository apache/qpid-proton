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

package org.apache.qpid.proton.transport2;

import java.util.List;
import java.util.Map;

import org.apache.qpid.proton.codec2.CodecHelper;
import org.apache.qpid.proton.codec2.DecodeException;
import org.apache.qpid.proton.codec2.DescribedTypeFactory;
import org.apache.qpid.proton.codec2.Encodable;
import org.apache.qpid.proton.codec2.Encoder;

public final class Flow implements Encodable, Performative
{
    public final static long CODE = 0x0000000000000013L;

    public final static String DESCRIPTOR = "amqp:flow:list";

    public final static Factory FACTORY = new Factory();

    private int _nextIncomingId;

    private int _incomingWindow;

    private int _nextOutgoingId;

    private int _outgoingWindow;

    private int _handle;

    private int _deliveryCount;

    private int _linkCredit;

    private int _available;

    private boolean _drain;

    private boolean _echo;

    private Map<Object, Object> _properties;

    public int getNextIncomingId()
    {
        return _nextIncomingId;
    }

    public void setNextIncomingId(int nextIncomingId)
    {
        _nextIncomingId = nextIncomingId;
    }

    public int getIncomingWindow()
    {
        return _incomingWindow;
    }

    public void setIncomingWindow(int incomingWindow)
    {
        _incomingWindow = incomingWindow;
    }

    public int getNextOutgoingId()
    {
        return _nextOutgoingId;
    }

    public void setNextOutgoingId(int nextOutgoingId)
    {
        _nextOutgoingId = nextOutgoingId;
    }

    public int getOutgoingWindow()
    {
        return _outgoingWindow;
    }

    public void setOutgoingWindow(int outgoingWindow)
    {
        _outgoingWindow = outgoingWindow;
    }

    public int getHandle()
    {
        return _handle;
    }

    public void setHandle(int handle)
    {
        _handle = handle;
    }

    public int getDeliveryCount()
    {
        return _deliveryCount;
    }

    public void setDeliveryCount(int deliveryCount)
    {
        _deliveryCount = deliveryCount;
    }

    public int getLinkCredit()
    {
        return _linkCredit;
    }

    public void setLinkCredit(int linkCredit)
    {
        _linkCredit = linkCredit;
    }

    public int getAvailable()
    {
        return _available;
    }

    public void setAvailable(int available)
    {
        _available = available;
    }

    public boolean getDrain()
    {
        return _drain;
    }

    public void setDrain(boolean drain)
    {
        _drain = drain;
    }

    public boolean getEcho()
    {
        return _echo;
    }

    public void setEcho(boolean echo)
    {
        _echo = echo;
    }

    public Map<Object, Object> getProperties()
    {
        return _properties;
    }

    public void setProperties(Map<Object, Object> properties)
    {
        _properties = properties;
    }

    @Override
    public void encode(Encoder encoder)
    {
        encoder.putDescriptor();
        encoder.putUlong(CODE);
        encoder.putList();
        encoder.putUint(_nextIncomingId);
        encoder.putUint(_incomingWindow);
        encoder.putUint(_nextOutgoingId);
        encoder.putUint(_outgoingWindow);
        encoder.putUint(_handle);
        encoder.putUint(_deliveryCount);
        encoder.putUint(_linkCredit);
        encoder.putUint(_available);
        encoder.putBoolean(_drain);
        encoder.putBoolean(_echo);
        CodecHelper.encodeMap(encoder, _properties);
        encoder.end();
    }

    public static final class Factory implements DescribedTypeFactory
    {
        @SuppressWarnings("unchecked")
        public Object create(Object in) throws DecodeException
        {
            List<Object> l = (List<Object>) in;

            Flow flow = new Flow();

            switch (11 - l.size())
            {

            case 0:
                flow.setProperties(((Map<Object, Object>) l.get(10)));
            case 1:
                flow.setEcho(l.get(9) == null ? false : (Boolean) l.get(9));
            case 2:
                flow.setDrain(l.get(8) == null ? false : (Boolean) l.get(8));
            case 3:
                flow.setAvailable(l.get(7) == null ? -1 : (Integer)l.get(7));
            case 4:
                flow.setLinkCredit((Integer) l.get(6));
            case 5:
                flow.setDeliveryCount((Integer) l.get(5));
            case 6:
                flow.setHandle((Integer) l.get(4));
            case 7:
                flow.setOutgoingWindow((Integer) l.get(3));
            case 8:
                flow.setNextOutgoingId((Integer) l.get(2));
            case 9:
                flow.setIncomingWindow((Integer) l.get(1));
            case 10:
                flow.setNextIncomingId((Integer) l.get(0));
            }

            return flow;
        }
    }

    @Override
    public String toString()
    {
        return "Flow{" +
                "nextIncomingId=" + _nextIncomingId +
                ", incomingWindow=" + _incomingWindow +
                ", nextOutgoingId=" + _nextOutgoingId +
                ", outgoingWindow=" + _outgoingWindow +
                ", handle=" + _handle +
                ", deliveryCount=" + _deliveryCount +
                ", linkCredit=" + _linkCredit +
                ", available=" + _available +
                ", drain=" + _drain +
                ", echo=" + _echo +
                ", properties=" + _properties +
                '}';
    }

    @Override
    public long getCode()
    {
        return CODE;
    }

    @Override
    public String getDescriptor()
    {
        return DESCRIPTOR;
    }
}