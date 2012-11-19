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

public interface WritableBuffer
{
    public void put(byte b);

    void putFloat(float f);

    void putDouble(double d);

    void put(byte[] src, int offset, int length);

    void putShort(short s);

    void putInt(int i);

    void putLong(long l);

    boolean hasRemaining();

    int remaining();

    int position();

    void position(int position);

    void put(ByteBuffer payload);

    int limit();

    class ByteBufferWrapper implements WritableBuffer
    {
        private final ByteBuffer _buf;

        public ByteBufferWrapper(ByteBuffer buf)
        {
            _buf = buf;
        }

        public void put(byte b)
        {
            _buf.put(b);
        }

        public void putFloat(float f)
        {
            _buf.putFloat(f);
        }

        public void putDouble(double d)
        {
            _buf.putDouble(d);
        }

        public void put(byte[] src, int offset, int length)
        {
            _buf.put(src, offset, length);
        }

        public void putShort(short s)
        {
            _buf.putShort(s);
        }

        public void putInt(int i)
        {
            _buf.putInt(i);
        }

        public void putLong(long l)
        {
            _buf.putLong(l);
        }

        public boolean hasRemaining()
        {
            return _buf.hasRemaining();
        }

        public int remaining()
        {
            return _buf.remaining();
        }

        public int position()
        {
            return _buf.position();
        }

        public void position(int position)
        {
            _buf.position(position);
        }

        public void put(ByteBuffer src)
        {
            _buf.put(src);
        }

        public int limit()
        {
            return _buf.limit();
        }

        @Override
        public String toString()
        {
            return String.format("[pos: %d, limit: %d, remaining:%d]", _buf.position(), _buf.limit(), _buf.remaining());
        }
    }
}
