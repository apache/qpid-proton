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

import java.util.List;

import org.apache.qpid.proton.codec2.DecodeException;
import org.apache.qpid.proton.codec2.DescribedTypeFactory;
import org.apache.qpid.proton.codec2.Encodable;
import org.apache.qpid.proton.codec2.Encoder;

public final class SaslResponse implements Encodable
{
    public final static long DESCRIPTOR_LONG = 0x0000000000000043L;

    public final static String DESCRIPTOR_STRING = "amqp:sasl-response:list";

    public final static Factory FACTORY = new Factory();

    private byte[] _response;

    public byte[] getResponse()
    {
        return _response;
    }

    public void setResponse(byte[] response)
    {
        if (response == null)
        {
            throw new NullPointerException("the response field is mandatory");
        }

        _response = response;
    }

    @Override
    public void encode(Encoder encoder)
    {
        encoder.putDescriptor();
        encoder.putUlong(DESCRIPTOR_LONG);
        encoder.putList();
        encoder.putBinary(_response, 0, _response.length);
        encoder.end();
    }

    public static final class Factory implements DescribedTypeFactory
    {
        @SuppressWarnings("unchecked")
        public Object create(Object in) throws DecodeException
        {
            List<Object> l = (List<Object>) in;
            SaslResponse saslResponse = new SaslResponse();

            if (l.size() > 0)
            {
                saslResponse.setResponse((byte[]) l.get(0));
            }

            return saslResponse;
        }
    }

    @Override
    public String toString()
    {
        return "SaslResponse{response=" + _response + '}';
    }
}