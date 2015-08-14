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

package org.apache.qpid.proton.security2;

import java.util.List;

import org.apache.qpid.proton.codec2.DecodeException;
import org.apache.qpid.proton.codec2.DescribedTypeFactory;
import org.apache.qpid.proton.codec2.Encodable;
import org.apache.qpid.proton.codec2.Encoder;

public final class SaslChallenge implements Encodable, SaslBody
{
    public final static long CODE = 0x0000000000000042L;

    public final static String DESCRIPTOR = "amqp:sasl-challenge:list";

    public final static Factory FACTORY = new Factory();

    private byte[] _challenge;

    public byte[] getChallenge()
    {
        return _challenge;
    }

    public void setChallenge(byte[] challenge)
    {
        if (challenge == null)
        {
            throw new NullPointerException("the challenge field is mandatory");
        }

        _challenge = challenge;
    }

    @Override
    public void encode(Encoder encoder)
    {
        encoder.putDescriptor();
        encoder.putUlong(CODE);
        encoder.putList();
        encoder.putBinary(_challenge, 0, _challenge.length);
        encoder.end();
    }

    public static final class Factory implements DescribedTypeFactory
    {
        @SuppressWarnings("unchecked")
        public Object create(Object in) throws DecodeException
        {
            List<Object> l = (List<Object>) in;
            SaslChallenge saslChallenge = new SaslChallenge();

            if (l.size() > 0)
            {
                saslChallenge.setChallenge((byte[]) l.get(0));
            }

            return saslChallenge;
        }
    }

    @Override
    public String toString()
    {
        return "SaslChallenge{" + "challenge=" + _challenge + '}';
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