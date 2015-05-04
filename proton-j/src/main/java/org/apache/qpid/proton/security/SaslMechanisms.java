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

package org.apache.qpid.proton.security;

import java.util.Arrays;
import java.util.List;

import org.apache.qpid.proton.codec2.DecodeException;
import org.apache.qpid.proton.codec2.DescribedTypeFactory;
import org.apache.qpid.proton.codec2.Encodable;
import org.apache.qpid.proton.codec2.Encoder;
import org.apache.qpid.proton.codec2.Type;

public final class SaslMechanisms implements Encodable
{
    public final static long DESCRIPTOR_LONG = 0x0000000000000040L;

    public final static String DESCRIPTOR_STRING = "amqp:sasl-mechanisms:list";

    public final static Factory FACTORY = new Factory();

    private String[] _saslServerMechanisms;

    public String[] getSaslServerMechanisms()
    {
        return _saslServerMechanisms;
    }

    public void setSaslServerMechanisms(String... saslServerMechanisms)
    {
        if (saslServerMechanisms == null)
        {
            throw new NullPointerException("the sasl-server-mechanisms field is mandatory");
        }

        _saslServerMechanisms = saslServerMechanisms;
    }

    @Override
    public void encode(Encoder encoder)
    {
        encoder.putDescriptor();
        encoder.putUlong(DESCRIPTOR_LONG);
        encoder.putList();
        encoder.putArray(Type.SYMBOL);
        for (String mech : _saslServerMechanisms)
        {
            encoder.putSymbol(mech);
        }
        encoder.end();
        encoder.end();
    }

    public static final class Factory implements DescribedTypeFactory
    {
        @SuppressWarnings("unchecked")
        public Object create(Object in) throws DecodeException
        {
            List<Object> l = (List<Object>) in;
            SaslMechanisms saslMech = new SaslMechanisms();

            if (l.isEmpty())
            {
                throw new DecodeException("The sasl-server-mechanisms field cannot be omitted");
            }

            Object val0 = l.get(0);
            if (val0 == null || val0.getClass().isArray())
            {
                saslMech.setSaslServerMechanisms((String[]) val0);
            }
            else
            {
                saslMech.setSaslServerMechanisms((String) val0);
            }
            return saslMech;
        }
    }

    @Override
    public String toString()
    {
        return "SaslMechanisms{" + "saslServerMechanisms="
                + (_saslServerMechanisms == null ? null : Arrays.asList(_saslServerMechanisms)) + '}';
    }
}