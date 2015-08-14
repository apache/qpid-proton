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

public final class SaslInit implements Encodable, SaslBody
{
    public final static long CODE = 0x0000000000000042L;

    public final static String DESCRIPTOR = "amqp:sasl-challenge:list";

    public final static Factory FACTORY = new Factory();

    private String _mechanism;

    private byte[] _initialResponse;

    private String _hostname;

    public String getMechanism()
    {
        return _mechanism;
    }

    public void setMechanism(String mechanism)
    {
        if (mechanism == null)
        {
            throw new NullPointerException("the mechanism field is mandatory");
        }

        _mechanism = mechanism;
    }

    public byte[] getInitialResponse()
    {
        return _initialResponse;
    }

    public void setInitialResponse(byte[] initialResponse)
    {
        _initialResponse = initialResponse;
    }

    public String getHostname()
    {
        return _hostname;
    }

    public void setHostname(String hostname)
    {
        _hostname = hostname;
    }

    @Override
    public void encode(Encoder encoder)
    {
        encoder.putDescriptor();
        encoder.putUlong(CODE);
        encoder.putList();
        encoder.putSymbol(_mechanism);
        encoder.putBinary(_initialResponse, 0, _initialResponse.length);
        encoder.putString(_hostname);
        encoder.end();
    }

    public static final class Factory implements DescribedTypeFactory
    {
        @SuppressWarnings("unchecked")
        public Object create(Object in) throws DecodeException
        {
            List<Object> l = (List<Object>) in;
            SaslInit saslInit = new SaslInit();

            switch (3 - l.size())
            {

            case 0:
                saslInit.setHostname((String) l.get(2));
            case 1:
                saslInit.setInitialResponse((byte[]) l.get(1));
            case 2:
                saslInit.setMechanism((String) l.get(0));
            }

            return saslInit;
        }
    }

    @Override
    public String toString()
    {
        return "SaslInit{" +
               "mechanism=" + _mechanism +
               ", initialResponse=" + _initialResponse +
               ", hostname='" + _hostname + '\'' +
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