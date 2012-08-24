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

import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.charset.CharacterCodingException;
import java.nio.charset.Charset;
import java.nio.charset.CharsetDecoder;
import java.util.Arrays;
import java.util.Collection;

public class StringType extends AbstractPrimitiveType<String>
{
    private static final Charset Charset_UTF8 = Charset.forName("UTF-8");
    private static final DecoderImpl.TypeDecoder<String> _stringCreator =
            new DecoderImpl.TypeDecoder<String>()
            {

                public String decode(final ByteBuffer buf)
                {
                    CharsetDecoder charsetDecoder = Charset_UTF8.newDecoder();
                    try
                    {
                        CharBuffer charBuf = charsetDecoder.decode(buf);
                        return charBuf.toString();
                    }
                    catch (CharacterCodingException e)
                    {
                        throw new IllegalArgumentException("Cannot parse String");
                    }

                }
            };


    public static interface StringEncoding extends PrimitiveTypeEncoding<String>
    {
        void setValue(String val, int length);
    }

    private final StringEncoding _stringEncoding;
    private final StringEncoding _shortStringEncoding;

    StringType(final EncoderImpl encoder, final DecoderImpl decoder)
    {
        _stringEncoding = new AllStringEncoding(encoder, decoder);
        _shortStringEncoding = new ShortStringEncoding(encoder, decoder);
        encoder.register(String.class, this);
        decoder.register(this);
    }

    public Class<String> getTypeClass()
    {
        return String.class;
    }

    public StringEncoding getEncoding(final String val)
    {
        final int length = calculateUTF8Length(val);
        StringEncoding encoding = length <= 255
                ? _shortStringEncoding
                : _stringEncoding;
        encoding.setValue(val, length);
        return encoding;
    }

    private static int calculateUTF8Length(final String s)
    {
        int len = s.length();
        int i = 0;
        final int length = s.length();
        while(i < length)
        {
            char c = s.charAt(i);
            if(c > 127)
            {
                len++;
                if(c > 0x07ff)
                {
                    len++;
                    if(c >= 0xD800 && c <= 0xDBFF)
                    {
                        i++;
                        len++;
                    }
                }
            }
            i++;

        }
        return len;
    }


    public StringEncoding getCanonicalEncoding()
    {
        return _stringEncoding;
    }

    public Collection<StringEncoding> getAllEncodings()
    {
        return Arrays.asList(_shortStringEncoding, _stringEncoding);
    }

    private class AllStringEncoding
            extends LargeFloatingSizePrimitiveTypeEncoding<String>
            implements StringEncoding
    {

        private String _value;
        private int _length;


        public AllStringEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
        {
            super(encoder, decoder);
        }

        @Override
        protected void writeEncodedValue(final String val)
        {
            getEncoder().writeRaw(val);
        }

        @Override
        protected int getEncodedValueSize(final String val)
        {
            return (val == _value) ? _length : calculateUTF8Length(val);
        }


        @Override
        public byte getEncodingCode()
        {
            return EncodingCodes.STR32;
        }

        public StringType getType()
        {
            return StringType.this;
        }

        public boolean encodesSuperset(final TypeEncoding<String> encoding)
        {
            return (getType() == encoding.getType());
        }

        public String readValue()
        {

            DecoderImpl decoder = getDecoder();
            int size = decoder.readRawInt();
            return decoder.readRaw(_stringCreator, size);
        }

        public void setValue(final String val, final int length)
        {
            _value = val;
            _length = length;
        }

    }

    private class ShortStringEncoding
            extends SmallFloatingSizePrimitiveTypeEncoding<String>
            implements StringEncoding
    {

        private String _value;
        private int _length;

        public ShortStringEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
        {
            super(encoder, decoder);
        }


        @Override
        protected void writeEncodedValue(final String val)
        {
            getEncoder().writeRaw(val);
        }

        @Override
        protected int getEncodedValueSize(final String val)
        {
            return (val == _value) ? _length : calculateUTF8Length(val);
        }


        @Override
        public byte getEncodingCode()
        {
            return EncodingCodes.STR8;
        }

        public StringType getType()
        {
            return StringType.this;
        }

        public boolean encodesSuperset(final TypeEncoding<String> encoder)
        {
            return encoder == this;
        }

        public String readValue()
        {

            DecoderImpl decoder = getDecoder();
            int size = ((int)decoder.readRawByte()) & 0xff;
            return decoder.readRaw(_stringCreator, size);
        }

        public void setValue(final String val, final int length)
        {
            _value = val;
            _length = length;
        }
    }

}
