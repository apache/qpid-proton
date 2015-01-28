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

import java.nio.charset.StandardCharsets;

/**
 * AbstractEncoder
 *
 */

public abstract class AbstractEncoder implements Encoder
{

    abstract void skip(int width);

    abstract void writeF8(int i);
    abstract void writeF16(int i);
    abstract void writeF32(int i);
    abstract void writeF64(long l);

    abstract void writeV8(byte[] bytes, int offset, int size);
    abstract void writeV32(byte[] bytes, int offset, int size);

    abstract int getPosition();
    abstract void setPosition(int i);

    private int count;

    private class Frame {
        Frame next;
        int start;
        int count;
        boolean written;
        Coder coder;
        Incrementor incrementor;
    }

    private Frame free = null;

    private Frame allocate() {
        if (free == null) {
            free = new Frame();
        }

        Frame frame = free;
        free = free.next;
        return frame;
    }

    private void free(Frame frame) {
        frame.next = free;
        free = frame;
    }

    private Frame current = null;

    private void push() {
        Frame frame = allocate();
        frame.next = current;
        frame.start = getPosition();
        frame.count = count;
        frame.coder = coder;
        frame.incrementor = incrementor;
        count = 0;
        incrementor = NOOP;
        coder = DEFAULT;
        current = frame;
    }

    private void pop() {
        count = current.count;
        coder = current.coder;
        incrementor = current.incrementor;
        current = current.next;
        if (current != null) {
            free(current);
        }
    }

    private abstract class Coder {
        abstract void write(int encoding);
    }

    private class DefaultCoder extends Coder {
        void write(int encoding) {
            writeF8(encoding);
        }
    }

    private class ArrayCoder extends Coder {
        void write(int encoding) {
            writeF8(encoding);
            coder = NOCODE;
        }
    }

    private class NoopCoder extends Coder {
        void write(int encoding) {}
    }

    private final Coder DEFAULT = new DefaultCoder();
    private final Coder ARRAY = new ArrayCoder();
    private final Coder NOCODE = new NoopCoder();
    private Coder coder = DEFAULT;

    private void writeCode(int encoding) {
        coder.write(encoding);
    }

    private abstract class Incrementor {
        abstract void go();
    }

    private class NoopIncrementor extends Incrementor {
        void go() {}
    }

    private class DescriptorIncrementor extends Incrementor {
        void go() {
            if (count == 2) {
                pop();
                increment();
                incrementor = NOOP;
            }
        }
    }

    private class DescriptorArrayIncrementor extends Incrementor {
        void go() {
            if (count == 1) {
                pop();
                incrementor = NOOP;
            }
        }
    }

    private final Incrementor NOOP = new NoopIncrementor();
    private final Incrementor DESC = new DescriptorIncrementor();
    private final Incrementor DESC_ARRAY = new DescriptorArrayIncrementor();

    private Incrementor incrementor = NOOP;

    private void increment() {
        count++;
        incrementor.go();
    }

    @Override
    public void putNull() {
        writeCode(Encodings.NULL);
        increment();
    }

    @Override
    public void putBoolean(boolean b) {
        // XXX: array
        if (b) {
            writeCode(Encodings.TRUE);
        } else {
            writeCode(Encodings.FALSE);
        }
        increment();
    }

    @Override
    public void putByte(byte b) {
        writeCode(Encodings.BYTE);
        writeF8(b);
        increment();
    }

    @Override
    public void putShort(short s) {
        writeCode(Encodings.SHORT);
        writeF16(s);
        increment();
    }

    @Override
    public void putInt(int i) {
        writeCode(Encodings.INT);
        writeF32(i);
        increment();
    }

    @Override
    public void putLong(long l) {
        writeCode(Encodings.LONG);
        writeF64(l);
        increment();
    }

    @Override
    public void putUbyte(byte b) {
        writeCode(Encodings.UBYTE);
        writeF8(b);
        increment();
    }

    @Override
    public void putUshort(short s) {
        writeCode(Encodings.USHORT);
        writeF16(s);
        increment();
    }

    @Override
    public void putUint(int i) {
        writeCode(Encodings.UINT);
        writeF32(i);
        increment();
    }

    @Override
    public void putUlong(long l) {
        writeCode(Encodings.ULONG);
        writeF64(l);
        increment();
    }

    @Override
    public void putFloat(float f) {
        writeCode(Encodings.FLOAT);
        writeF32(Float.floatToIntBits(f));
        increment();
    }

    @Override
    public void putDouble(double d) {
        writeCode(Encodings.DOUBLE);
        writeF64(Double.doubleToLongBits(d));
        increment();
    }

    @Override
    public void putChar(char c) {
        putChar(c);
    }

    @Override
    public void putChar(int utf32) {
        writeCode(Encodings.UTF32);
        writeF32(utf32);
        increment();
    }

    @Override
    public void putTimestamp(long t) {
        writeCode(Encodings.MS64);
        writeF64(t);
        increment();
    }

    @Override
    public void putUUID(long hi, long lo) {
        writeCode(Encodings.UUID);
        writeF64(hi);
        writeF64(lo);
        increment();
    }

    @Override
    public void putString(String s) {
        byte[] bytes = s.getBytes(StandardCharsets.UTF_8);
        putString(bytes, 0, bytes.length);
    }

    @Override
    public void putString(byte[] utf8, int offset, int size) {
        writeCode(Encodings.STR32);
        writeV32(utf8, offset, size);
        increment();
    }

    @Override
    public void putBinary(byte[] bytes, int offset, int size) {
        writeCode(Encodings.VBIN32);
        writeV32(bytes, offset, size);
        increment();
    }

    @Override
    public void putSymbol(String s) {
        byte[] bytes = s.getBytes(StandardCharsets.US_ASCII);
        putSymbol(bytes, 0, bytes.length);
    }

    @Override
    public void putSymbol(byte[] ascii, int offset, int size) {
        writeCode(Encodings.SYM32);
        writeV32(ascii, offset, size);
        increment();
    }

    private void start(int width) {
        push();
        skip(width);
    }

    @Override
    public void putList() {
        writeCode(Encodings.LIST32);
        start(Widths.LIST32);
    }

    @Override
    public void putMap() {
        writeCode(Encodings.MAP32);
        start(Widths.MAP32);
    }

    @Override
    public void putArray(Type t) {
        writeCode(Encodings.ARRAY32);
        start(Widths.ARRAY32);
        coder = ARRAY;
    }

    @Override
    public void putDescriptor() {
        writeF8(0x0);
        Incrementor inc = coder == ARRAY ? DESC_ARRAY : DESC;
        push();
        incrementor = inc;
    }

    @Override
    public void end() {
        if (current == null) {
            throw new IllegalStateException("mismatched call to end()");
        }
        int pos = getPosition();
        setPosition(current.start);
        writeF32(pos - current.start);
        writeF32(count);
        setPosition(pos);
        pop();
        increment();
    }

}
