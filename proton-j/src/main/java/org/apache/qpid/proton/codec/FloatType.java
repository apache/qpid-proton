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

public class FloatType extends AbstractPrimitiveType<Float>
{
    private FloatEncoding _floatEncoding;

    FloatType(final EncoderImpl encoder, final DecoderImpl decoder)
    {
        _floatEncoding = new FloatEncoding(encoder, decoder);
        encoder.register(Float.class, this);
        decoder.register(this);
    }

    public Class<Float> getTypeClass()
    {
        return Float.class;
    }

    public FloatEncoding getEncoding(final Float val)
    {
        return _floatEncoding;
    }


    public FloatEncoding getCanonicalEncoding()
    {
        return _floatEncoding;
    }

    public Collection<FloatEncoding> getAllEncodings()
    {
        return Collections.singleton(_floatEncoding);
    }

    public void write(WritableBuffer buffer, float f)
    {
        _floatEncoding.write(buffer, f);
    }
    
    public class FloatEncoding extends FixedSizePrimitiveTypeEncoding<Float>
    {

        public FloatEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
        {
            super(encoder, decoder);
        }

        @Override
        protected int getFixedSize()
        {
            return 4;
        }

        @Override
        public byte getEncodingCode()
        {
            return EncodingCodes.FLOAT;
        }

        public FloatType getType()
        {
            return FloatType.this;
        }

        public void writeValue(WritableBuffer buffer, final Float val)
        {
            getEncoder().writeRaw(buffer, val.floatValue());
        }

        public void writeValue(WritableBuffer buffer, final float val)
        {
            getEncoder().writeRaw(buffer, val);
        }


        public void write(WritableBuffer buffer, final float f)
        {
            writeConstructor(buffer);
            getEncoder().writeRaw(buffer, f);
            
        }

        public boolean encodesSuperset(final TypeEncoding<Float> encoding)
        {
            return (getType() == encoding.getType());
        }

        public Float readValue(ReadableBuffer buffer)
        {
            return readPrimitiveValue(buffer);
        }

        public float readPrimitiveValue(ReadableBuffer buffer)
        {
            return getDecoder().readRawFloat(buffer);
        }


        @Override
        public boolean encodesJavaPrimitive()
        {
            return true;
        }
    }
}
