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

public final class Target extends Terminus implements Encodable
{
    public final static long DESCRIPTOR_LONG = 0x0000000000000029L;

    public final static String DESCRIPTOR_STRING = "amqp:target:list";

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
        CodecHelper.encodeSymbolArray(encoder, _capabilities);
        encoder.end();
    }

    public static final class Factory implements DescribedTypeFactory
    {
        @SuppressWarnings("unchecked")
        public Object create(Object in) throws DecodeException
        {
            List<Object> l = (List<Object>) in;

            Target target = new Target();

            switch (7 - l.size())
            {

            case 0:
                Object val0 = l.get(6);
                if (val0 == null || val0.getClass().isArray())
                {
                    target.setCapabilities((String[]) val0);
                }
                else
                {
                    target.setCapabilities((String) val0);
                }
            case 1:
                target.setDynamicNodeProperties((Map<Object, Object>) l.get(5));
            case 2:
                target.setDynamic(l.get(4) == null ? false : (Boolean) l.get(4));
            case 3:
                target.setTimeout(l.get(3) == null ? 0 : (Integer) l.get(3));
            case 4:
                target.setExpiryPolicy(l.get(2) == null ? TerminusExpiryPolicy.SESSION_END : TerminusExpiryPolicy
                        .getEnum((String) l.get(2)));
            case 5:
                target.setDurable(l.get(1) == null ? TerminusDurability.NONE : TerminusDurability.get((Byte) l.get(1)));
            case 6:
                target.setAddress((String) l.get(0));
            }
            return target;
        }
    }

    @Override
    public String toString()
    {
        return "Target{" +
               "address='" + getAddress() + '\'' +
               ", durable=" + getDurable() +
               ", expiryPolicy=" + getExpiryPolicy() +
               ", timeout=" + getTimeout() +
               ", dynamic=" + getDynamic() +
               ", dynamicNodeProperties=" + getDynamicNodeProperties() +
               ", capabilities=" + (getCapabilities() == null ? null : Arrays.asList(getCapabilities())) +
               '}';
    }
}