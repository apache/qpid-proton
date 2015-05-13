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
import org.apache.qpid.proton.message2.Accepted.Factory;

public final class Source extends Terminus implements Encodable
{
    public final static long DESCRIPTOR_LONG = 0x0000000000000028L;

    public final static String DESCRIPTOR_STRING = "amqp:source:list";

    public final static Factory FACTORY = new Factory();

    private String _distributionMode;

    private Map<Object, Object> _filter;

    private Outcome _defaultOutcome;

    private String[] _outcomes;

    public String getDistributionMode()
    {
        return _distributionMode;
    }

    public void setDistributionMode(String distributionMode)
    {
        _distributionMode = distributionMode;
    }

    public Map<Object, Object> getFilter()
    {
        return _filter;
    }

    public void setFilter(Map<Object, Object> filter)
    {
        _filter = filter;
    }

    public Outcome getDefaultOutcome()
    {
        return _defaultOutcome;
    }

    public void setDefaultOutcome(Outcome defaultOutcome)
    {
        _defaultOutcome = defaultOutcome;
    }

    public String[] getOutcomes()
    {
        return _outcomes;
    }

    public void setOutcomes(String... outcomes)
    {
        _outcomes = outcomes;
    }

    @Override
    public void encode(Encoder encoder)
    {
        encoder.putDescriptor();
        encoder.putUlong(DESCRIPTOR_LONG);
        encoder.putList();
        encoder.putString(_address);
        encoder.putByte(_durable.getValue());
        encoder.putSymbol(_expiryPolicy.getPolicy());
        encoder.putLong(_timeout);
        encoder.putBoolean(_dynamic);
        CodecHelper.encodeMap(encoder, _dynamicNodeProperties);
        encoder.putSymbol(_distributionMode);
        CodecHelper.encodeMap(encoder, _filter);
        if (_defaultOutcome == null)
        {
            encoder.putNull();
        }
        else
        {
            _defaultOutcome.encode(encoder);
        }
        CodecHelper.encodeSymbolArray(encoder, _outcomes);
        CodecHelper.encodeSymbolArray(encoder, _capabilities);
        encoder.end();
    }

    public static final class Factory implements DescribedTypeFactory
    {
        @SuppressWarnings("unchecked")
        public Object create(Object in) throws DecodeException
        {
            List<Object> l = (List<Object>) in;

            Source source = new Source();

            switch (11 - l.size())
            {

            case 0:
                Object val0 = l.get(10);
                if (val0 == null || val0.getClass().isArray())
                {
                    source.setCapabilities((String[]) val0);
                }
                else
                {
                    source.setCapabilities((String) val0);
                }
            case 1:
                Object val1 = l.get(9);
                if (val1 == null || val1.getClass().isArray())
                {
                    source.setOutcomes((String[]) val1);
                }
                else
                {
                    source.setOutcomes((String) val1);
                }
            case 2:
                source.setDefaultOutcome((Outcome) l.get(8));
            case 3:
                source.setFilter((Map<Object, Object>) l.get(7));
            case 4:
                source.setDistributionMode((String) l.get(6));
            case 5:
                source.setDynamicNodeProperties((Map<Object, Object>) l.get(5));
            case 6:
                Boolean dynamic = (Boolean) l.get(4);
                source.setDynamic(dynamic == null ? false : dynamic);
            case 7:
                source.setTimeout(l.get(3) == null ? 0 : (Long) l.get(3));
            case 8:
                source.setExpiryPolicy(l.get(2) == null ? TerminusExpiryPolicy.SESSION_END : TerminusExpiryPolicy
                        .getEnum((String) l.get(2)));
            case 9:
                source.setDurable(l.get(1) == null ? TerminusDurability.NONE : TerminusDurability.get((Byte) l.get(1)));
            case 10:
                source.setAddress((String) l.get(0));
            }

            return source;
        }
    }

    @Override
    public String toString()
    {
        return "Source{" + "address='" + getAddress() + '\'' + ", durable=" + getDurable() + ", expiryPolicy="
                + getExpiryPolicy() + ", timeout=" + getTimeout() + ", dynamic=" + getDynamic()
                + ", dynamicNodeProperties=" + getDynamicNodeProperties() + ", distributionMode=" + _distributionMode
                + ", filter=" + _filter + ", defaultOutcome=" + _defaultOutcome + ", outcomes="
                + (_outcomes == null ? null : Arrays.asList(_outcomes)) + ", capabilities="
                + (getCapabilities() == null ? null : Arrays.asList(getCapabilities())) + '}';
    }
}