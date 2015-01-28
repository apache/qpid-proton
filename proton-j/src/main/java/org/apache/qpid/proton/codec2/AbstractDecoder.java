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
 * AbstractDecoder
 *
 */

public abstract class AbstractDecoder implements Decoder
{

    int start;
    int offset;
    int limit;

    private int code;
    private int size;
    private int count;

    abstract int readF8(int offset);

    abstract int readF16(int offset);

    abstract int readF32(int offset);

    abstract long readF64(int offset);

    abstract byte[] readBytes(int offset, int size);

    public void decode(DataHandler handler)
    {
        while (offset < limit) {
            decodeType(handler);
            decodeValue(handler);
        }
    }

    private void decodeType(DataHandler handler)
    {
        code = readF8(offset++);
        if (code == 0) {
            handler.onDescriptor(this);
            decodeType(handler);
            decodeValue(handler);
            decodeType(handler);
        }
    }

    private void decodeValue(DataHandler handler)
    {
        int copy;

        switch (code) {
        case Encodings.NULL:
            handler.onNull(this);
            offset += Widths.NULL;
            break;
        case Encodings.BOOLEAN:
            handler.onBoolean(this);
            offset += Widths.BOOLEAN;
            break;
        case Encodings.TRUE:
            handler.onBoolean(this);
            offset += Widths.TRUE;
            break;
        case Encodings.FALSE:
            handler.onBoolean(this);
            offset += Widths.FALSE;
            break;
        case Encodings.UBYTE:
            handler.onUbyte(this);
            offset += Widths.UBYTE;
            break;
        case Encodings.USHORT:
            handler.onUshort(this);
            offset += Widths.USHORT;
            break;
        case Encodings.UINT:
            handler.onUint(this);
            offset += Widths.UINT;
            break;
        case Encodings.SMALLUINT:
            handler.onUint(this);
            offset += Widths.SMALLUINT;
            break;
        case Encodings.UINT0:
            handler.onUint(this);
            offset += Widths.UINT0;
            break;
        case Encodings.ULONG:
            handler.onUlong(this);
            offset += Widths.ULONG;
            break;
        case Encodings.SMALLULONG:
            handler.onUlong(this);
            offset += Widths.SMALLULONG;
            break;
        case Encodings.ULONG0:
            handler.onUlong(this);
            offset += Widths.ULONG0;
            break;
        case Encodings.BYTE:
            handler.onByte(this);
            offset += Widths.BYTE;
            break;
        case Encodings.SHORT:
            handler.onShort(this);
            offset += Widths.SHORT;
            break;
        case Encodings.INT:
            handler.onInt(this);
            offset += Widths.INT;
            break;
        case Encodings.SMALLINT:
            handler.onInt(this);
            offset += Widths.SMALLINT;
            break;
        case Encodings.LONG:
            handler.onLong(this);
            offset += Widths.LONG;
            break;
        case Encodings.SMALLLONG:
            handler.onLong(this);
            offset += Widths.SMALLLONG;
            break;
        case Encodings.FLOAT:
            handler.onFloat(this);
            offset += Widths.FLOAT;
            break;
        case Encodings.DOUBLE:
            handler.onDouble(this);
            offset += Widths.DOUBLE;
            break;
        case Encodings.DECIMAL32:
            handler.onDecimal32(this);
            offset += Widths.DECIMAL32;
            break;
        case Encodings.DECIMAL64:
            handler.onDecimal64(this);
            offset += Widths.DECIMAL64;
            break;
        case Encodings.DECIMAL128:
            handler.onDecimal128(this);
            offset += Widths.DECIMAL128;
            break;
        case Encodings.UTF32:
            handler.onChar(this);
            offset += Widths.UTF32;
            break;
        case Encodings.MS64:
            handler.onTimestamp(this);
            offset += Widths.MS64;
            break;
        case Encodings.UUID:
            handler.onUUID(this);
            offset += Widths.UUID;
            break;
        case Encodings.VBIN8:
            count = size = readF8(offset);
            offset += Widths.VBIN8;
            handler.onBinary(this);
            offset += size;
            break;
        case Encodings.VBIN32:
            count = size = readF32(offset);
            offset += Widths.VBIN32;
            handler.onBinary(this);
            offset += size;
            break;
        case Encodings.STR8:
            count = size = readF8(offset);
            offset += Widths.STR8;
            handler.onString(this);
            offset += size;
            break;
        case Encodings.STR32:
            count = size = readF32(offset);
            offset += Widths.STR32;
            handler.onString(this);
            offset += size;
            break;
        case Encodings.SYM8:
            count = size = readF8(offset);
            offset += Widths.SYM8;
            handler.onSymbol(this);
            offset += size;
            break;
        case Encodings.SYM32:
            count = size = readF32(offset);
            offset += Widths.SYM32;
            handler.onSymbol(this);
            offset += size;
            break;
        case Encodings.LIST0:
            count = 0;
            handler.onList(this);
            offset += Widths.LIST0;
            handler.onListEnd(this);
            break;
        case Encodings.LIST8:
            count = readF8(offset + 1);
            handler.onList(this);
            offset += Widths.LIST8;
            decodeCompound(handler);
            handler.onListEnd(this);
            break;
        case Encodings.LIST32:
            count = readF32(offset + 4);
            handler.onList(this);
            offset += Widths.LIST32;
            decodeCompound(handler);
            handler.onListEnd(this);
            break;
        case Encodings.MAP8:
            count = readF8(offset + 1);
            handler.onMap(this);
            offset += Widths.MAP8;
            decodeCompound(handler);
            handler.onMapEnd(this);
            break;
        case Encodings.MAP32:
            count = readF32(offset + 4);
            handler.onMap(this);
            offset += Widths.MAP32;
            decodeCompound(handler);
            handler.onMapEnd(this);
            break;
        case Encodings.ARRAY8:
            count = readF8(offset + 1);
            handler.onArray(this);
            offset += Widths.ARRAY8;
            decodeArray(handler);
            handler.onArrayEnd(this);
            break;
        case Encodings.ARRAY32:
            count = readF32(offset + 4);
            handler.onArray(this);
            offset += Widths.ARRAY32;
            decodeArray(handler);
            handler.onArrayEnd(this);
            break;
        }
    }

    private void decodeCompound(DataHandler handler) {
        int copy = count;
        for (int i = 0; i < copy; i++) {
            decodeType(handler);
            decodeValue(handler);
        }
    }

    private void decodeArray(DataHandler handler) {
        int copy = count;
        decodeType(handler);
        for (int i = 0; i < copy; i++) {
            decodeValue(handler);
        }
    }

    @Override
    public Type getType() {
        return Type.typeOf(code);
    }

    @Override
    public int getSize() {
        return count;
    }

    @Override
    public int getInt() {
        switch (code) {
        case Encodings.NULL:
        case Encodings.FALSE:
            return 0;
        case Encodings.TRUE:
            return 1;
        case Encodings.BOOLEAN:
        case Encodings.BYTE:
        case Encodings.UBYTE:
        case Encodings.SMALLINT:
        case Encodings.SMALLLONG:
        case Encodings.SMALLUINT:
        case Encodings.SMALLULONG:
            return readF8(offset);
        case Encodings.SHORT:
        case Encodings.USHORT:
            return readF16(offset);
        case Encodings.INT:
        case Encodings.UINT:
        case Encodings.UTF32:
            return readF32(offset);
        case Encodings.LONG:
        case Encodings.ULONG:
        case Encodings.MS64:
            return (int) readF64(offset);
        case Encodings.FLOAT:
            return (int) Float.intBitsToFloat(readF32(offset));
        case Encodings.DOUBLE:
            return (int) Double.longBitsToDouble(readF64(offset));
        default:
            throw new IllegalStateException("cannot convert to an int: " + Type.typeOf(code));
        }
    }

    @Override
    public long getLong() {
        switch (code) {
        case Encodings.NULL:
        case Encodings.FALSE:
            return 0;
        case Encodings.TRUE:
            return 1;
        case Encodings.BOOLEAN:
        case Encodings.BYTE:
        case Encodings.UBYTE:
        case Encodings.SMALLINT:
        case Encodings.SMALLLONG:
        case Encodings.SMALLUINT:
        case Encodings.SMALLULONG:
            return readF8(offset);
        case Encodings.SHORT:
        case Encodings.USHORT:
            return readF16(offset);
        case Encodings.INT:
        case Encodings.UINT:
        case Encodings.UTF32:
            return readF32(offset);
        case Encodings.LONG:
        case Encodings.ULONG:
        case Encodings.MS64:
            return readF64(offset);
        case Encodings.FLOAT:
            return (long) Float.intBitsToFloat(readF32(offset));
        case Encodings.DOUBLE:
            return (long) Double.longBitsToDouble(readF64(offset));
        default:
            throw new IllegalStateException("cannot convert to a long: " + Type.typeOf(code));
        }
    }

    @Override
    public float getFloat() {
        switch (code) {
        case Encodings.NULL:
        case Encodings.FALSE:
            return 0;
        case Encodings.TRUE:
            return 1;
        case Encodings.BOOLEAN:
        case Encodings.BYTE:
        case Encodings.UBYTE:
        case Encodings.SMALLINT:
        case Encodings.SMALLLONG:
        case Encodings.SMALLUINT:
        case Encodings.SMALLULONG:
            return readF8(offset);
        case Encodings.SHORT:
        case Encodings.USHORT:
            return readF16(offset);
        case Encodings.INT:
        case Encodings.UINT:
        case Encodings.UTF32:
            return readF32(offset);
        case Encodings.LONG:
        case Encodings.ULONG:
        case Encodings.MS64:
            return readF64(offset);
        case Encodings.FLOAT:
            return Float.intBitsToFloat(readF32(offset));
        case Encodings.DOUBLE:
            return (float) Double.longBitsToDouble(readF64(offset));
        default:
            throw new IllegalStateException("cannot convert to a float: " + Type.typeOf(code));
        }
    }

    @Override
    public double getDouble() {
        switch (code) {
        case Encodings.NULL:
        case Encodings.FALSE:
            return 0;
        case Encodings.TRUE:
            return 1;
        case Encodings.BOOLEAN:
        case Encodings.BYTE:
        case Encodings.UBYTE:
        case Encodings.SMALLINT:
        case Encodings.SMALLLONG:
        case Encodings.SMALLUINT:
        case Encodings.SMALLULONG:
            return readF8(offset);
        case Encodings.SHORT:
        case Encodings.USHORT:
            return readF16(offset);
        case Encodings.INT:
        case Encodings.UINT:
        case Encodings.UTF32:
            return readF32(offset);
        case Encodings.LONG:
        case Encodings.ULONG:
        case Encodings.MS64:
            return readF64(offset);
        case Encodings.FLOAT:
            return Float.intBitsToFloat(readF32(offset));
        case Encodings.DOUBLE:
            return Double.longBitsToDouble(readF64(offset));
        default:
            throw new IllegalStateException("cannot convert to a double: " + Type.typeOf(code));
        }
    }

    @Override
    public String getString() {
        switch (code) {
        case Encodings.SYM32:
        case Encodings.STR32:
            return new String(readBytes(offset, size), StandardCharsets.UTF_8);
        default:
            throw new IllegalStateException("cannot convert to a string: " + Type.typeOf(code));
        }
    }

    @Override
    public int getIntBits() {
        return readF32(offset);
    }

    @Override
    public long getLongBits() {
        return readF64(offset);
    }

    @Override
    public long getHiBits() {
        return readF64(offset);
    }

    @Override
    public long getLoBits() {
        return readF64(offset + 8);
    }

}
