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

import org.apache.qpid.proton.amqp.UnsignedByte;

import java.util.Collection;
import java.util.Collections;

public class UnsignedByteType extends AbstractPrimitiveType<UnsignedByte>
{
    private UnsignedByteEncoding _unsignedByteEncoding;

    UnsignedByteType(final EncoderImpl encoder, final DecoderImpl decoder)
    {
        _unsignedByteEncoding = new UnsignedByteEncoding(encoder, decoder);
        encoder.register(UnsignedByte.class, this);
        decoder.register(this);
    }

    public Class<UnsignedByte> getTypeClass()
    {
        return UnsignedByte.class;
    }

    public UnsignedByteEncoding getEncoding(final UnsignedByte val)
    {
        return _unsignedByteEncoding;
    }


    public UnsignedByteEncoding getCanonicalEncoding()
    {
        return _unsignedByteEncoding;
    }

    public Collection<UnsignedByteEncoding> getAllEncodings()
    {
        return Collections.singleton(_unsignedByteEncoding);
    }

    public class UnsignedByteEncoding extends FixedSizePrimitiveTypeEncoding<UnsignedByte>
    {

        public UnsignedByteEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
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
            return EncodingCodes.UBYTE;
        }

        public UnsignedByteType getType()
        {
            return UnsignedByteType.this;
        }

        public void writeValue(final UnsignedByte val)
        {
            getEncoder().writeRaw(val.byteValue());
        }

        public boolean encodesSuperset(final TypeEncoding<UnsignedByte> encoding)
        {
            return (getType() == encoding.getType());
        }

        public UnsignedByte readValue()
        {
            return UnsignedByte.valueOf(getDecoder().readRawByte());
        }
    }
}
