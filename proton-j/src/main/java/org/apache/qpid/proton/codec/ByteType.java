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

public class ByteType extends AbstractPrimitiveType<Byte>
{
    private ByteEncoding _byteEncoding;

    ByteType(final EncoderImpl encoder, final DecoderImpl decoder)
    {
        _byteEncoding = new ByteEncoding(encoder, decoder);
        encoder.register(Byte.class, this);
        decoder.register(this);
    }

    public Class<Byte> getTypeClass()
    {
        return Byte.class;
    }

    public ByteEncoding getEncoding(final Byte val)
    {
        return _byteEncoding;
    }


    public ByteEncoding getCanonicalEncoding()
    {
        return _byteEncoding;
    }

    public Collection<ByteEncoding> getAllEncodings()
    {
        return Collections.singleton(_byteEncoding);
    }

    public void writeType(byte b)
    {
        _byteEncoding.write(b);
    }


    public class ByteEncoding extends FixedSizePrimitiveTypeEncoding<Byte>
    {

        public ByteEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
        {
            super(encoder, decoder);
        }

        @Override
        protected int getFixedSize()
        {
            return 1;
        }

        @Override
        public byte getEncodingCode()
        {
            return EncodingCodes.BYTE;
        }

        public ByteType getType()
        {
            return ByteType.this;
        }

        public void writeValue(final Byte val)
        {
            getEncoder().writeRaw(val);
        }


        public void write(final byte val)
        {
            writeConstructor();
            getEncoder().writeRaw(val);
        }

        public void writeValue(final byte val)
        {
            getEncoder().writeRaw(val);
        }

        public boolean encodesSuperset(final TypeEncoding<Byte> encoding)
        {
            return (getType() == encoding.getType());
        }

        public Byte readValue()
        {
            return readPrimitiveValue();
        }

        public byte readPrimitiveValue()
        {
            return getDecoder().readRawByte();
        }


        @Override
        public boolean encodesJavaPrimitive()
        {
            return true;
        }

    }
}
