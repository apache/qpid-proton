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

public final class SaslOutcome implements Encodable, SaslBody
{
    public final static long CODE = 0x0000000000000044L;

    public final static String DESCRIPTOR = "amqp:sasl-saslOutcome:list";

    public final static Factory FACTORY = new Factory();

    private SaslCode _code;

    private byte[] _additionalData;

    public SaslCode getSaslCode()
    {
        return _code;
    }

    public void setSaslCode(SaslCode code)
    {
        if (code == null)
        {
            throw new NullPointerException("the code field is mandatory");
        }

        _code = code;
    }

    public byte[] getAdditionalData()
    {
        return _additionalData;
    }

    public void setAdditionalData(byte[] additionalData)
    {
        _additionalData = additionalData;
    }

    @Override
    public void encode(Encoder encoder)
    {
        encoder.putDescriptor();
        encoder.putUlong(CODE);
        encoder.putList();
        encoder.putUbyte(_code.getValue());
        if (_additionalData != null)
        {
            encoder.putBinary(_additionalData, 0, _additionalData.length);
        }
        encoder.end();
    }

    public static final class Factory implements DescribedTypeFactory
    {
        @SuppressWarnings("unchecked")
        public Object create(Object in) throws DecodeException
        {
            List<Object> l = (List<Object>) in;
            SaslOutcome saslOutcome = new SaslOutcome();

            if(l.isEmpty())
            {
                throw new DecodeException("The code field cannot be omitted");
            }

            switch(2 - l.size())
            {

                case 0:
                    saslOutcome.setAdditionalData( (byte[]) l.get( 1 ) );
                case 1:
                    saslOutcome.setSaslCode(SaslCode.valueOf((Byte)l.get(0)));
            }


            return saslOutcome;
        }
    }

    @Override
    public String toString()
    {
        return "SaslOutcome{" +
               "_code=" + _code +
               ", _additionalData=" + _additionalData +
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