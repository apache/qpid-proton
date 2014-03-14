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

public final class NullType extends AbstractPrimitiveType<Void>
{
    private NullEncoding _nullEncoding;

    NullType(final EncoderImpl encoder, final DecoderImpl decoder)
    {
        _nullEncoding = new NullEncoding(encoder, decoder);
        encoder.register(Void.class, this);
        decoder.register(this);
    }

    public Class<Void> getTypeClass()
    {
        return Void.class;
    }

    public NullEncoding getEncoding(final Void val)
    {
        return _nullEncoding;
    }


    public NullEncoding getCanonicalEncoding()
    {
        return _nullEncoding;
    }

    public Collection<NullEncoding> getAllEncodings()
    {
        return Collections.singleton(_nullEncoding);
    }

    public void write()
    {
        _nullEncoding.write();
    }

    private class NullEncoding extends FixedSizePrimitiveTypeEncoding<Void>
    {

        public NullEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
        {
            super(encoder, decoder);
        }

        @Override
        protected int getFixedSize()
        {
            return 0;
        }

        @Override
        public byte getEncodingCode()
        {
            return EncodingCodes.NULL;
        }

        public NullType getType()
        {
            return NullType.this;
        }

        public void writeValue(final Void val)
        {
        }

        public void writeValue()
        {
        }

        public boolean encodesSuperset(final TypeEncoding<Void> encoding)
        {
            return encoding == this;
        }

        public Void readValue()
        {
            return null;
        }

        public void write()
        {
            writeConstructor();
        }
    }
}
