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

public class DoubleType extends AbstractPrimitiveType<Double>
{
    private DoubleEncoding _doubleEncoding;

    DoubleType(final EncoderImpl encoder, final DecoderImpl decoder)
    {
        _doubleEncoding = new DoubleEncoding(encoder, decoder);
        encoder.register(Double.class, this);
        decoder.register(this);
    }

    public Class<Double> getTypeClass()
    {
        return Double.class;
    }

    public DoubleEncoding getEncoding(final Double val)
    {
        return _doubleEncoding;
    }


    public DoubleEncoding getCanonicalEncoding()
    {
        return _doubleEncoding;
    }

    public Collection<DoubleEncoding> getAllEncodings()
    {
        return Collections.singleton(_doubleEncoding);
    }

    public void write(double d)
    {
        _doubleEncoding.write(d);
    }
    
    public class DoubleEncoding extends FixedSizePrimitiveTypeEncoding<Double>
    {

        public DoubleEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
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
            return EncodingCodes.DOUBLE;
        }

        public DoubleType getType()
        {
            return DoubleType.this;
        }

        public void writeValue(final Double val)
        {
            getEncoder().writeRaw(val.doubleValue());
        }

        public void writeValue(final double val)
        {
            getEncoder().writeRaw(val);
        }

        public void write(final double d)
        {
            writeConstructor();
            getEncoder().writeRaw(d);
            
        }

        public boolean encodesSuperset(final TypeEncoding<Double> encoding)
        {
            return (getType() == encoding.getType());
        }

        public Double readValue()
        {
            return readPrimitiveValue();
        }

        public double readPrimitiveValue()
        {
            return getDecoder().readRawDouble();
        }


        @Override
        public boolean encodesJavaPrimitive()
        {
            return true;
        }
    }
}
