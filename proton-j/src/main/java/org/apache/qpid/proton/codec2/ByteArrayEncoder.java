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
 * ByteArrayEncoder
 *
 */

public class ByteArrayEncoder extends AbstractEncoder2
{

    private byte[] bytes;
    private int start;
    private int offset;
    private int limit;

    public ByteArrayEncoder() {}

    public void init(byte[] bytes, int offset, int size)
    {
        this.bytes = bytes;
        this.start = offset;
        this.offset = offset;
        this.limit = size + offset;
    }

    public int getPosition()
    {
        return offset;
    }

    public void setPosition(int p)
    {
        offset = p;
    }

    void skip(int size)
    {
        offset += size;
    }

    void writeF8(int value) {
        bytes[offset++] = (byte) (0xFF & (value));
    }

    void writeF16(int value) {
        bytes[offset++] = (byte) (0xFF & (value >> 8));
        bytes[offset++] = (byte) (0xFF & (value));
    }

    void writeF32(int value) {
        bytes[offset++] = (byte) (0xFF & (value >> 24));
        bytes[offset++] = (byte) (0xFF & (value >> 16));
        bytes[offset++] = (byte) (0xFF & (value >> 8));
        bytes[offset++] = (byte) (0xFF & (value));
    }

    void writeF64(long value) {
        bytes[offset++] = (byte) (0xFF & (value >> 56));
        bytes[offset++] = (byte) (0xFF & (value >> 48));
        bytes[offset++] = (byte) (0xFF & (value >> 40));
        bytes[offset++] = (byte) (0xFF & (value >> 32));
        bytes[offset++] = (byte) (0xFF & (value >> 24));
        bytes[offset++] = (byte) (0xFF & (value >> 16));
        bytes[offset++] = (byte) (0xFF & (value >>  8));
        bytes[offset++] = (byte) (0xFF & (value      ));
    }

    void writeV8(byte[] bytes, int offset, int size) {
        writeF8(size);
        System.arraycopy(bytes, offset, this.bytes, this.offset, size);
        this.offset += size;
    }

    void writeV32(byte[] bytes, int offset, int size) {
        writeF32(size);
        System.arraycopy(bytes, offset, this.bytes, this.offset, size);
        this.offset += size;
    }
}
