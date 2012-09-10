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
import java.util.UUID;

public class UUIDType extends AbstractPrimitiveType<UUID>
{
    private UUIDEncoding _uuidEncoding;

    UUIDType(final EncoderImpl encoder, final DecoderImpl decoder)
    {
        _uuidEncoding = new UUIDEncoding(encoder, decoder);
        encoder.register(UUID.class, this);
        decoder.register(this);
    }

    public Class<UUID> getTypeClass()
    {
        return UUID.class;
    }

    public UUIDEncoding getEncoding(final UUID val)
    {
        return _uuidEncoding;
    }


    public UUIDEncoding getCanonicalEncoding()
    {
        return _uuidEncoding;
    }

    public Collection<UUIDEncoding> getAllEncodings()
    {
        return Collections.singleton(_uuidEncoding);
    }

    private class UUIDEncoding extends FixedSizePrimitiveTypeEncoding<UUID>
    {

        public UUIDEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
        {
            super(encoder, decoder);
        }

        @Override
        protected int getFixedSize()
        {
            return 16;
        }

        @Override
        public byte getEncodingCode()
        {
            return EncodingCodes.UUID;
        }

        public UUIDType getType()
        {
            return UUIDType.this;
        }

        public void writeValue(final UUID val)
        {
            getEncoder().writeRaw(val.getMostSignificantBits());
            getEncoder().writeRaw(val.getLeastSignificantBits());
        }

        public boolean encodesSuperset(final TypeEncoding<UUID> encoding)
        {
            return (getType() == encoding.getType());
        }

        public UUID readValue()
        {
            long msb = getDecoder().readRawLong();
            long lsb = getDecoder().readRawLong();

            return new UUID(msb, lsb);
        }
    }
}
