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

public class CharacterType extends AbstractPrimitiveType<Character>
{
    private CharacterEncoding _characterEncoding;

    CharacterType(final EncoderImpl encoder, final DecoderImpl decoder)
    {
        _characterEncoding = new CharacterEncoding(encoder, decoder);
        encoder.register(Character.class, this);
        decoder.register(this);
    }

    public Class<Character> getTypeClass()
    {
        return Character.class;
    }

    public CharacterEncoding getEncoding(final Character val)
    {
        return _characterEncoding;
    }


    public CharacterEncoding getCanonicalEncoding()
    {
        return _characterEncoding;
    }

    public Collection<CharacterEncoding> getAllEncodings()
    {
        return Collections.singleton(_characterEncoding);
    }

    public void write(char c)
    {
        _characterEncoding.write(c);
    }

    public class CharacterEncoding extends FixedSizePrimitiveTypeEncoding<Character>
    {

        public CharacterEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
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
            return EncodingCodes.CHAR;
        }

        public CharacterType getType()
        {
            return CharacterType.this;
        }

        public void writeValue(final Character val)
        {
            getEncoder().writeRaw((int)val.charValue() & 0xffff);
        }

        public void writeValue(final char val)
        {
            getEncoder().writeRaw((int)val & 0xffff);
        }

        public void write(final char c)
        {
            writeConstructor();
            getEncoder().writeRaw((int)c & 0xffff);

        }

        public boolean encodesSuperset(final TypeEncoding<Character> encoding)
        {
            return (getType() == encoding.getType());
        }

        public Character readValue()
        {
            return readPrimitiveValue();
        }

        public char readPrimitiveValue()
        {
            return (char) (getDecoder().readRawInt() & 0xffff);
        }


        @Override
        public boolean encodesJavaPrimitive()
        {
            return true;
        }
    }
}
