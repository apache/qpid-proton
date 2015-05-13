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

import java.util.Arrays;
import java.util.List;
import java.util.Map;

import org.apache.qpid.proton.codec2.CodecHelper;
import org.apache.qpid.proton.codec2.DecodeException;
import org.apache.qpid.proton.codec2.DescribedTypeFactory;
import org.apache.qpid.proton.codec2.Encodable;
import org.apache.qpid.proton.codec2.Encoder;

public final class Begin implements Encodable
{
    public final static long DESCRIPTOR_LONG = 0x0000000000000012L;

    public final static String DESCRIPTOR_STRING = "amqp:begin:list";

    public final static Factory FACTORY = new Factory();

    private int _remoteChannel = -1;

    private int _nextOutgoingId;

    private int _incomingWindow;

    private int _outgoingWindow;

    private int _handleMax = 0xffffffff;

    private String[] _offeredCapabilities;

    private String[] _desiredCapabilities;

    private Map<String, Object> _properties;

    public int getRemoteChannel()
    {
        return _remoteChannel;
    }

    public void setRemoteChannel(int remoteChannel)
    {
        _remoteChannel = remoteChannel;
    }

    public long getNextOutgoingId()
    {
        return _nextOutgoingId;
    }

    public void setNextOutgoingId(int nextOutgoingId)
    {
        _nextOutgoingId = nextOutgoingId;
    }

    public int getIncomingWindow()
    {
        return _incomingWindow;
    }

    public void setIncomingWindow(int incomingWindow)
    {
        _incomingWindow = incomingWindow;
    }

    public int getOutgoingWindow()
    {
        return _outgoingWindow;
    }

    public void setOutgoingWindow(int outgoingWindow)
    {
        _outgoingWindow = outgoingWindow;
    }

    public int getHandleMax()
    {
        return _handleMax;
    }

    public void setHandleMax(int handleMax)
    {
        _handleMax = handleMax;
    }

    public String[] getOfferedCapabilities()
    {
        return _offeredCapabilities;
    }

    public void setOfferedCapabilities(String... offeredCapabilities)
    {
        _offeredCapabilities = offeredCapabilities;
    }

    public String[] getDesiredCapabilities()
    {
        return _desiredCapabilities;
    }

    public void setDesiredCapabilities(String... desiredCapabilities)
    {
        _desiredCapabilities = desiredCapabilities;
    }

    public Map<String, Object> getProperties()
    {
        return _properties;
    }

    public void setProperties(Map<String, Object> properties)
    {
        _properties = properties;
    }

    @Override
    public void encode(Encoder encoder)
    {
        encoder.putDescriptor();
        encoder.putUlong(DESCRIPTOR_LONG);
        encoder.putList();
        encoder.putUshort(_remoteChannel);
        encoder.putUint(_nextOutgoingId);
        encoder.putUint(_incomingWindow);
        encoder.putUint(_outgoingWindow);
        encoder.putUint(_handleMax);
        CodecHelper.encodeSymbolArray(encoder, _offeredCapabilities);
        CodecHelper.encodeSymbolArray(encoder, _desiredCapabilities);
        CodecHelper.encodeMapWithKeyAsSymbol(encoder, _properties);
        encoder.end();
    }

    public static final class Factory implements DescribedTypeFactory
    {
        @SuppressWarnings("unchecked")
        public Object create(Object in) throws DecodeException
        {
            List<Object> l = (List<Object>) in;
            Begin begin = new Begin();

            switch (8 - l.size())
            {

            case 0:
                begin.setProperties((Map<String, Object>) l.get(7));
            case 1:
                Object val1 = l.get(6);
                if (val1 == null || val1.getClass().isArray())
                {
                    begin.setDesiredCapabilities((String[]) val1);
                }
                else
                {
                    begin.setDesiredCapabilities((String) val1);
                }
            case 2:
                Object val2 = l.get(5);
                if (val2 == null || val2.getClass().isArray())
                {
                    begin.setOfferedCapabilities((String[]) val2);
                }
                else
                {
                    begin.setOfferedCapabilities((String) val2);
                }
            case 3:
                begin.setHandleMax(l.get(4) == null ? 0xffffffff : (Integer) l.get(4));
            case 4:
                begin.setOutgoingWindow((Integer) l.get(3));
            case 5:
                begin.setIncomingWindow((Integer) l.get(2));
            case 6:
                begin.setNextOutgoingId((Integer) l.get(1));
            case 7:
                begin.setRemoteChannel((Integer) l.get(0));
            }

            return begin;
        }
    }

    @Override
    public String toString()
    {
        return "Begin{" +
                "remoteChannel=" + _remoteChannel +
                ", nextOutgoingId=" + _nextOutgoingId +
                ", incomingWindow=" + _incomingWindow +
                ", outgoingWindow=" + _outgoingWindow +
                ", handleMax=" + _handleMax +
                ", offeredCapabilities=" + (_offeredCapabilities == null ? null : Arrays.asList(_offeredCapabilities)) +
                ", desiredCapabilities=" + (_desiredCapabilities == null ? null : Arrays.asList(_desiredCapabilities)) +
                ", properties=" + _properties +
                '}';
    }
}