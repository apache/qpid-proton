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
package org.apache.qpid.proton.codec;

import java.util.Collection;
import java.util.Collections;
import java.util.Date;

public class TimestampType extends AbstractPrimitiveType<Date>
{
    private TimestampEncoding _timestampEncoding;

    TimestampType(final EncoderImpl encoder, final DecoderImpl decoder)
    {
        _timestampEncoding = new TimestampEncoding(encoder, decoder);
        encoder.register(Date.class, this);
        decoder.register(this);
    }

    public Class<Date> getTypeClass()
    {
        return Date.class;
    }

    public TimestampEncoding getEncoding(final Date val)
    {
        return _timestampEncoding;
    }


    public TimestampEncoding getCanonicalEncoding()
    {
        return _timestampEncoding;
    }

    public Collection<TimestampEncoding> getAllEncodings()
    {
        return Collections.singleton(_timestampEncoding);
    }

    public void write(long l)
    {
        _timestampEncoding.write(l);
    }
    
    private class TimestampEncoding extends FixedSizePrimitiveTypeEncoding<Date>
    {

        public TimestampEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
        {
            super(encoder, decoder);
        }

        @Override
        protected int getFixedSize()
        {
            return 8;
        }

        @Override
        public byte getEncodingCode()
        {
            return EncodingCodes.TIMESTAMP;
        }

        public TimestampType getType()
        {
            return TimestampType.this;
        }

        public void writeValue(final Date val)
        {
            getEncoder().writeRaw(val.getTime());
        }
        
        public void write(final long l)
        {
            writeConstructor();
            getEncoder().writeRaw(l);
            
        }

        public boolean encodesSuperset(final TypeEncoding<Date> encoding)
        {
            return (getType() == encoding.getType());
        }

        public Date readValue()
        {
            return new Date(getDecoder().readRawLong());
        }
    }
}
