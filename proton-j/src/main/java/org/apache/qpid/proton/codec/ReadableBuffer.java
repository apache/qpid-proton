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

/**
 * Interface to abstract a buffer, just like {@link WritableBuffer}
 */
public interface ReadableBuffer
{

    void put(ReadableBuffer other);

    byte get();

    int getInt();

    long getLong();

    short getShort();

    float getFloat();

    double getDouble();

    ReadableBuffer get(final byte[] data, final int offset, final int length);

    ReadableBuffer get(final byte[] data);

    ReadableBuffer position(int position);

    ReadableBuffer slice();

    ReadableBuffer flip();

    ReadableBuffer limit(int limit);

    int limit();

    int remaining();

    int position();

    boolean hasRemaining();

    ReadableBuffer duplicate();

    ByteBuffer byteBuffer();



    String readUTF8();

    class ByteBufferReader implements ReadableBuffer
    {
        ByteBuffer buffer;

        public static ByteBufferReader allocate(int size)
        {
            ByteBuffer allocated = ByteBuffer.allocate(size);
            return new ByteBufferReader(allocated);
        }

        public ByteBufferReader(ByteBuffer buffer)
        {
            this.buffer = buffer;
        }
        @Override
        public byte get()
        {
            return buffer.get();
        }

        @Override
        public int getInt()
        {
            return buffer.getInt();
        }

        @Override
        public long getLong()
        {
            return buffer.getLong();
        }

        @Override
        public short getShort()
        {
            return buffer.getShort();
        }

        @Override
        public float getFloat()
        {
            return buffer.getFloat();
        }

        @Override
        public double getDouble()
        {
            return buffer.getDouble();
        }

        @Override
        public int limit()
        {
            return buffer.limit();
        }

        @Override
        public ReadableBuffer get(byte[] data, int offset, int length)
        {
            buffer.get(data, offset, length);
            return this;
        }

        @Override
        public ReadableBuffer get(byte[] data)
        {
            buffer.get(data);
            return this;
        }

        @Override
        public ReadableBuffer flip()
        {
            buffer.flip();
            return this;
        }


        @Override
        public ReadableBuffer position(int position)
        {
            buffer.position(position);
            return this;
        }

        @Override
        public ReadableBuffer slice()
        {
            return new ByteBufferReader(buffer.slice());
        }

        @Override
        public ReadableBuffer limit(int limit)
        {
            buffer.limit(limit);
            return this;
        }

        @Override
        public int remaining()
        {
            return buffer.remaining();
        }

        @Override
        public int position()
        {
            return buffer.position();
        }

        @Override
        public boolean hasRemaining()
        {
            return buffer.hasRemaining();
        }

        @Override
        public ReadableBuffer duplicate()
        {
            return new ByteBufferReader(buffer.duplicate());
        }

        @Override
        public ByteBuffer byteBuffer()
        {
            return buffer;
        }

     // IN our tests on HornetQ this is pretty bad performance wise
        private static final Charset Charset_UTF8 = Charset.forName("UTF-8");

        @Override
        public String readUTF8()
        {

            // By Clebert : This is the original implementation very if this is ok before we commit this.
            // I have actually a better UTF8 decoder on HornetQ, as this one sucks!!!!!!!

//            CharsetDecoder charsetDecoder = Charset_UTF8.newDecoder();
//            try
//            {
//                CharBuffer charBuf = charsetDecoder.decode(buffer);
//                return charBuf.toString();
//            }
//            catch (CharacterCodingException e)
//            {
//                throw new IllegalArgumentException("Cannot parse String");
//            }

            CharBuffer charBuf = Charset_UTF8.decode(buffer);
            return charBuf.toString();
        }

        public void put(ReadableBuffer other)
        {
            this.buffer.put(other.byteBuffer());
        }
    }




    class Test
    {
        public void main()
        {
            ByteBuffer buffer = null;
            buffer.getLong();
        }
    }

}
