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

public final class Open implements Encodable, Performative
{
    public final static long CODE = 0x0000000000000010L;

    public final static String DESCRIPTOR = "amqp:open:list";

    public final static Factory FACTORY = new Factory();

    private String _containerId;

    private String _hostname;

    private int _maxFrameSize = 0xffffffff;

    private int _channelMax = 65535;

    private long _idleTimeOut;

    private String[] _outgoingLocales;

    private String[] _incomingLocales;

    private String[] _offeredCapabilities;

    private String[] _desiredCapabilities;

    private Map<String, Object> _properties;

    public String getContainerId()
    {
        return _containerId;
    }

    public void setContainerId(String containerId)
    {
        if (containerId == null)
        {
            throw new NullPointerException("the container-id field is mandatory");
        }

        _containerId = containerId;
    }

    public String getHostname()
    {
        return _hostname;
    }

    public void setHostname(String hostname)
    {
        _hostname = hostname;
    }

    public int getMaxFrameSize()
    {
        return _maxFrameSize;
    }

    public void setMaxFrameSize(int maxFrameSize)
    {
        _maxFrameSize = maxFrameSize;
    }

    public int getChannelMax()
    {
        return _channelMax;
    }

    public void setChannelMax(int channelMax)
    {
        _channelMax = channelMax;
    }

    public long getIdleTimeOut()
    {
        return _idleTimeOut;
    }

    public void setIdleTimeOut(long idleTimeOut)
    {
        _idleTimeOut = idleTimeOut;
    }

    public String[] getOutgoingLocales()
    {
        return _outgoingLocales;
    }

    public void setOutgoingLocales(String... outgoingLocales)
    {
        _outgoingLocales = outgoingLocales;
    }

    public String[] getIncomingLocales()
    {
        return _incomingLocales;
    }

    public void setIncomingLocales(String... incomingLocales)
    {
        _incomingLocales = incomingLocales;
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
        encoder.putUlong(CODE);
        encoder.putList();
        encoder.putString(_containerId);
        encoder.putString(_hostname);
        encoder.putUint(_maxFrameSize);
        encoder.putUshort(_channelMax);
        encoder.putLong(_idleTimeOut);
        CodecHelper.encodeSymbolArray(encoder, _outgoingLocales);
        CodecHelper.encodeSymbolArray(encoder, _incomingLocales);
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

            Open open = new Open();

            try
            {
                switch (10 - l.size())
                {
                case 0:
                    open.setProperties((Map<String, Object>) l.get(9));
                case 1:
                    Object val1 = l.get(8);
                    if (val1 == null || val1.getClass().isArray())
                    {
                        open.setDesiredCapabilities((String[]) val1);
                    }
                    else
                    {
                        open.setDesiredCapabilities((String) val1);
                    }
                case 2:
                    Object val2 = l.get(7);
                    if (val2 == null || val2.getClass().isArray())
                    {
                        open.setOfferedCapabilities((String[]) val2);
                    }
                    else
                    {
                        open.setOfferedCapabilities((String) val2);
                    }
                case 3:
                    Object val3 = l.get(6);
                    if (val3 == null || val3.getClass().isArray())
                    {
                        open.setIncomingLocales((String[]) val3);
                    }
                    else
                    {
                        open.setIncomingLocales((String) val3);
                    }
                case 4:
                    Object val4 = l.get(5);
                    if (val4 == null || val4.getClass().isArray())
                    {
                        open.setOutgoingLocales((String[]) val4);
                    }
                    else
                    {
                        open.setOutgoingLocales((String) val4);
                    }
                case 5:
                    open.setIdleTimeOut(l.get(4) == null ? 0 : (long) l.get(4));
                case 6:
                    open.setChannelMax(l.get(3) == null ? 65535 : (short) l.get(3));
                case 7:
                    open.setMaxFrameSize(l.get(2) == null ? Integer.MAX_VALUE : (int) l.get(2));
                case 8:
                    open.setHostname((String) l.get(1));
                case 9:
                    open.setContainerId((String) l.get(0));
                }
            }
            catch (Exception e)
            {
                throw new DecodeException("Error decoding 'Open' performative", e);
            }

            return open;
        }
    }

    @Override
    public String toString()
    {
        return "Open{" +
                " containerId='" + _containerId + '\'' +
                ", hostname='" + _hostname + '\'' +
                ", maxFrameSize=" + _maxFrameSize +
                ", channelMax=" + _channelMax +
                ", idleTimeOut=" + _idleTimeOut +
                ", outgoingLocales=" + (_outgoingLocales == null ? null : Arrays.asList(_outgoingLocales)) +
                ", incomingLocales=" + (_incomingLocales == null ? null : Arrays.asList(_incomingLocales)) +
                ", offeredCapabilities=" + (_offeredCapabilities == null ? null : Arrays.asList(_offeredCapabilities)) +
                ", desiredCapabilities=" + (_desiredCapabilities == null ? null : Arrays.asList(_desiredCapabilities)) +
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