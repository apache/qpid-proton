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
package org.apache.qpid.proton.codec2;

/**
 * ByteArrayDecoder
 *
 */

public final class ByteArrayDecoder extends AbstractDecoder
{

    private byte[] bytes;

    public ByteArrayDecoder() {}

    public void init(byte[] bytes, int offset, int size) {
        this.bytes = bytes;
        this.start = offset;
        this.offset = offset;
        this.limit = offset + size;
    }

    @Override
    int readF8(int offset) {
        return 0xFF & bytes[offset];
    }

    @Override
    int readF16(int offset) {
        int a = 0xFF & bytes[offset + 0];
        int b = 0xFF & bytes[offset + 1];
        int value = a << 8 | b;
        return value;
    }

    @Override
    int readF32(int offset) {
        int a = 0xFF & bytes[offset + 0];
        int b = 0xFF & bytes[offset + 1];
        int c = 0xFF & bytes[offset + 2];
        int d = 0xFF & bytes[offset + 3];
        int value = a << 24 | b << 16 | c << 8 | d;
        return value;
    }

    @Override
    long readF64(int offset) {
        long a = 0xFFFFFFFFL & readF32(offset);
        long b = 0xFFFFFFFFL & readF32(offset + 4);
        long value = a << 32 | b;
        return value;
    }

    @Override
    byte[] readBytes(int offset, int size) {
        byte[] result = new byte[size];
        System.arraycopy(bytes, offset, result, 0, size);
        return result;
    }

}
