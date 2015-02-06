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

package org.apache.qpid.proton.transport;

import java.util.Arrays;
import java.util.List;
import java.util.Map;

import org.apache.qpid.proton.codec2.CodecHelper;
import org.apache.qpid.proton.codec2.DecodeException;
import org.apache.qpid.proton.codec2.DescribedTypeFactory;
import org.apache.qpid.proton.codec2.Encodable;
import org.apache.qpid.proton.codec2.Encoder;

public final class Attach implements Encodable
{
    public final static long DESCRIPTOR_LONG = 0x0000000000000012L;

    public final static String DESCRIPTOR_STRING = "amqp:attach:list";

    private String _name;

    private int _handle;

    private Role _role = Role.SENDER;

    private SenderSettleMode _sndSettleMode = SenderSettleMode.MIXED;

    private ReceiverSettleMode _rcvSettleMode = ReceiverSettleMode.FIRST;

    private Source _source;

    private Target _target;

    private Map<Object, Object> _unsettled;

    private boolean _incompleteUnsettled;

    private int _initialDeliveryCount;

    private long _maxMessageSize;

    private String[] _offeredCapabilities;

    private String[] _desiredCapabilities;

    private Map<Object, Object> _properties;

    public String getName()
    {
        return _name;
    }

    public void setName(String name)
    {
        if (name == null)
        {
            throw new NullPointerException("the name field is mandatory");
        }

        _name = name;
    }

    public int getHandle()
    {
        return _handle;
    }

    public void setHandle(int handle)
    {
        _handle = handle;
    }

    public Role getRole()
    {
        return _role;
    }

    public void setRole(Role role)
    {
        if (role == null)
        {
            throw new NullPointerException("Role cannot be null");
        }
        _role = role;
    }

    public SenderSettleMode getSndSettleMode()
    {
        return _sndSettleMode;
    }

    public void setSndSettleMode(SenderSettleMode sndSettleMode)
    {
        _sndSettleMode = sndSettleMode == null ? SenderSettleMode.MIXED : sndSettleMode;
    }

    public ReceiverSettleMode getRcvSettleMode()
    {
        return _rcvSettleMode;
    }

    public void setRcvSettleMode(ReceiverSettleMode rcvSettleMode)
    {
        _rcvSettleMode = rcvSettleMode == null ? ReceiverSettleMode.FIRST : rcvSettleMode;
    }

    public Source getSource()
    {
        return _source;
    }

    public void setSource(Source source)
    {
        _source = source;
    }

    public Target getTarget()
    {
        return _target;
    }

    public void setTarget(Target target)
    {
        _target = target;
    }

    public Map<Object, Object> getUnsettled()
    {
        return _unsettled;
    }

    public void setUnsettled(Map<Object, Object> unsettled)
    {
        _unsettled = unsettled;
    }

    public boolean getIncompleteUnsettled()
    {
        return _incompleteUnsettled;
    }

    public void setIncompleteUnsettled(boolean incompleteUnsettled)
    {
        _incompleteUnsettled = incompleteUnsettled;
    }

    public int getInitialDeliveryCount()
    {
        return _initialDeliveryCount;
    }

    public void setInitialDeliveryCount(int initialDeliveryCount)
    {
        _initialDeliveryCount = initialDeliveryCount;
    }

    public long getMaxMessageSize()
    {
        return _maxMessageSize;
    }

    public void setMaxMessageSize(long maxMessageSize)
    {
        _maxMessageSize = maxMessageSize;
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
        encoder.putUlong(DESCRIPTOR_LONG);
        encoder.putList();
        encoder.putString(_name);
        encoder.putLong(_handle);
        encoder.putBoolean(_role.getValue());
        encoder.putUbyte(_sndSettleMode.getValue());
        encoder.putUbyte(_rcvSettleMode.getValue());
        _source.encode(encoder);
        _target.encode(encoder);
        CodecHelper.encodeMap(encoder, _unsettled);
        encoder.putBoolean(_incompleteUnsettled);
        encoder.putLong(_initialDeliveryCount);
        encoder.putLong(_maxMessageSize);
        CodecHelper.encodeSymbolArray(encoder, _offeredCapabilities);
        CodecHelper.encodeSymbolArray(encoder, _desiredCapabilities);
        CodecHelper.encodeMap(encoder, _properties);
        encoder.end();
    }

    public static final class Factory implements DescribedTypeFactory
    {
        @SuppressWarnings("unchecked")
        public Object create(Object in) throws DecodeException
        {
            List<Object> l = (List<Object>) in;
            Attach attach = new Attach();

            switch (14 - l.size())
            {

            case 0:
                attach.setProperties((Map<Object, Object>) l.get(13));
            case 1:
                Object val1 = l.get(12);
                if (val1 == null || val1.getClass().isArray())
                {
                    attach.setDesiredCapabilities((String[]) val1);
                }
                else
                {
                    attach.setDesiredCapabilities((String) val1);
                }
            case 2:
                Object val2 = l.get(11);
                if (val2 == null || val2.getClass().isArray())
                {
                    attach.setOfferedCapabilities((String[]) val2);
                }
                else
                {
                    attach.setOfferedCapabilities((String) val2);
                }
            case 3:
                attach.setMaxMessageSize((long) l.get(10));
            case 4:
                attach.setInitialDeliveryCount((int) l.get(9));
            case 5:
                Boolean incompleteUnsettled = (Boolean) l.get(8);
                attach.setIncompleteUnsettled(incompleteUnsettled == null ? false : incompleteUnsettled);
            case 6:
                attach.setUnsettled((Map<Object, Object>) l.get(7));
            case 7:
                attach.setTarget((Target) l.get(6));
            case 8:
                attach.setSource((Source) l.get(5));
            case 9:
                attach.setRcvSettleMode(l.get(4) == null ? ReceiverSettleMode.FIRST
                        : ReceiverSettleMode.values()[(int) l.get(4)]);
            case 10:
                attach.setSndSettleMode(l.get(3) == null ? SenderSettleMode.MIXED : SenderSettleMode.values()[(int) l
                        .get(3)]);
            case 11:
                attach.setRole(Boolean.TRUE.equals(l.get(2)) ? Role.RECEIVER : Role.SENDER);
            case 12:
                attach.setHandle((int) l.get(1));
            case 13:
                attach.setName((String) l.get(0));
            }

            return attach;
        }
    }

    @Override
    public String toString()
    {
        return "Attach{" +
               "name='" + _name + '\'' +
               ", handle=" + _handle +
               ", role=" + _role +
               ", sndSettleMode=" + _sndSettleMode +
               ", rcvSettleMode=" + _rcvSettleMode +
               ", source=" + _source +
               ", target=" + _target +
               ", unsettled=" + _unsettled +
               ", incompleteUnsettled=" + _incompleteUnsettled +
               ", initialDeliveryCount=" + _initialDeliveryCount +
               ", maxMessageSize=" + _maxMessageSize +
               ", offeredCapabilities=" + (_offeredCapabilities == null ? null : Arrays.asList(_offeredCapabilities)) +
               ", desiredCapabilities=" + (_desiredCapabilities == null ? null : Arrays.asList(_desiredCapabilities)) +
               ", properties=" + _properties +
               '}';
    }
}