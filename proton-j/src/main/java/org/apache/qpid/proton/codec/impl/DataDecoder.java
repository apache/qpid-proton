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

package org.apache.qpid.proton.codec.impl;

import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.util.Date;
import java.util.UUID;

import org.apache.qpid.proton.amqp.*;
import org.apache.qpid.proton.codec.Data;

class DataDecoder
{

    private static final Charset ASCII = Charset.forName("US-ASCII");
    private static final Charset UTF_8 = Charset.forName("UTF-8");

    private static final TypeConstructor[] _constructors = new TypeConstructor[256];

    static
    {

        _constructors[0x00] = new DescribedTypeConstructor();

        _constructors[0x40] = new NullConstructor();
        _constructors[0x41] = new TrueConstructor();
        _constructors[0x42] = new FalseConstructor();
        _constructors[0x43] = new UInt0Constructor();
        _constructors[0x44] = new ULong0Constructor();
        _constructors[0x45] = new EmptyListConstructor();

        _constructors[0x50] = new UByteConstructor();
        _constructors[0x51] = new ByteConstructor();
        _constructors[0x52] = new SmallUIntConstructor();
        _constructors[0x53] = new SmallULongConstructor();
        _constructors[0x54] = new SmallIntConstructor();
        _constructors[0x55] = new SmallLongConstructor();
        _constructors[0x56] = new BooleanConstructor();

        _constructors[0x60] = new UShortConstructor();
        _constructors[0x61] = new ShortConstructor();

        _constructors[0x70] = new UIntConstructor();
        _constructors[0x71] = new IntConstructor();
        _constructors[0x72] = new FloatConstructor();
        _constructors[0x73] = new CharConstructor();
        _constructors[0x74] = new Decimal32Constructor();

        _constructors[0x80] = new ULongConstructor();
        _constructors[0x81] = new LongConstructor();
        _constructors[0x82] = new DoubleConstructor();
        _constructors[0x83] = new TimestampConstructor();
        _constructors[0x84] = new Decimal64Constructor();

        _constructors[0x94] = new Decimal128Constructor();
        _constructors[0x98] = new UUIDConstructor();

        _constructors[0xa0] = new SmallBinaryConstructor();
        _constructors[0xa1] = new SmallStringConstructor();
        _constructors[0xa3] = new SmallSymbolConstructor();

        _constructors[0xb0] = new BinaryConstructor();
        _constructors[0xb1] = new StringConstructor();
        _constructors[0xb3] = new SymbolConstructor();

        _constructors[0xc0] = new SmallListConstructor();
        _constructors[0xc1] = new SmallMapConstructor();


        _constructors[0xd0] = new ListConstructor();
        _constructors[0xd1] = new MapConstructor();

        _constructors[0xe0] = new SmallArrayConstructor();
        _constructors[0xf0] = new ArrayConstructor();

    }

    private interface TypeConstructor
    {
        Data.DataType getType();

        int size(ByteBuffer b);

        void parse(ByteBuffer b, Data data);
    }


    static int decode(ByteBuffer b, Data data)
    {
        if(b.hasRemaining())
        {
            int position = b.position();
            TypeConstructor c = readConstructor(b);
            final int size = c.size(b);
            if(b.remaining() >= size)
            {
                c.parse(b, data);
                return 1+size;
            }
            else
            {
                b.position(position);
                return -4;
            }
        }
        return 0;
    }

    private static TypeConstructor readConstructor(ByteBuffer b)
    {
        int index = b.get() & 0xff;
        TypeConstructor tc = _constructors[index];
        if(tc == null)
        {
            throw new IllegalArgumentException("No constructor for type " + index);
        }
        return tc;
    }


    private static class NullConstructor implements TypeConstructor
    {
        @Override
        public Data.DataType getType()
        {
            return Data.DataType.NULL;
        }

        @Override
        public int size(ByteBuffer b)
        {
            return 0;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putNull();
        }
    }

    private static class TrueConstructor implements TypeConstructor
    {
        @Override
        public Data.DataType getType()
        {
            return Data.DataType.BOOL;
        }

        @Override
        public int size(ByteBuffer b)
        {
            return 0;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putBoolean(true);
        }
    }


    private static class FalseConstructor implements TypeConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.BOOL;
        }

        @Override
        public int size(ByteBuffer b)
        {
            return 0;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putBoolean(false);
        }
    }

    private static class UInt0Constructor implements TypeConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.UINT;
        }

        @Override
        public int size(ByteBuffer b)
        {
            return 0;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putUnsignedInteger(UnsignedInteger.ZERO);
        }
    }

    private static class ULong0Constructor implements TypeConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.ULONG;
        }

        @Override
        public int size(ByteBuffer b)
        {
            return 0;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putUnsignedLong(UnsignedLong.ZERO);
        }
    }


    private static class EmptyListConstructor implements TypeConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.LIST;
        }

        @Override
        public int size(ByteBuffer b)
        {
            return 0;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putList();
        }
    }


    private static abstract class Fixed0SizeConstructor implements TypeConstructor
    {
        @Override
        public final int size(ByteBuffer b)
        {
            return 0;
        }
    }

    private static abstract class Fixed1SizeConstructor implements TypeConstructor
    {
        @Override
        public int size(ByteBuffer b)
        {
            return 1;
        }
    }

    private static abstract class Fixed2SizeConstructor implements TypeConstructor
    {
        @Override
        public int size(ByteBuffer b)
        {
            return 2;
        }
    }

    private static abstract class Fixed4SizeConstructor implements TypeConstructor
    {
        @Override
        public int size(ByteBuffer b)
        {
            return 4;
        }
    }

    private static abstract class Fixed8SizeConstructor implements TypeConstructor
    {
        @Override
        public int size(ByteBuffer b)
        {
            return 8;
        }
    }

    private static abstract class Fixed16SizeConstructor implements TypeConstructor
    {
        @Override
        public int size(ByteBuffer b)
        {
            return 16;
        }
    }

    private static class UByteConstructor extends Fixed1SizeConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.UBYTE;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putUnsignedByte(UnsignedByte.valueOf(b.get()));
        }
    }

    private static class ByteConstructor extends Fixed1SizeConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.BYTE;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putByte(b.get());
        }
    }

    private static class SmallUIntConstructor extends Fixed1SizeConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.UINT;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putUnsignedInteger(UnsignedInteger.valueOf(((int) b.get()) & 0xff));
        }
    }

    private static class SmallIntConstructor extends Fixed1SizeConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.INT;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putInt(b.get());
        }
    }

    private static class SmallULongConstructor extends Fixed1SizeConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.ULONG;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putUnsignedLong(UnsignedLong.valueOf(((int) b.get()) & 0xff));
        }
    }

    private static class SmallLongConstructor extends Fixed1SizeConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.LONG;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putLong(b.get());
        }
    }

    private static class BooleanConstructor extends Fixed1SizeConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.BOOL;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            int i = b.get();
            if(i != 0 && i != 1)
            {
                throw new IllegalArgumentException("Illegal value " + i + " for boolean");
            }
            data.putBoolean(i == 1);
        }
    }

    private static class UShortConstructor extends Fixed2SizeConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.USHORT;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putUnsignedShort(UnsignedShort.valueOf(b.getShort()));
        }
    }

    private static class ShortConstructor extends Fixed2SizeConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.SHORT;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putShort(b.getShort());
        }
    }

    private static class UIntConstructor extends Fixed4SizeConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.UINT;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putUnsignedInteger(UnsignedInteger.valueOf(b.getInt()));
        }
    }

    private static class IntConstructor extends Fixed4SizeConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.INT;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putInt(b.getInt());
        }
    }

    private static class FloatConstructor extends Fixed4SizeConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.FLOAT;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putFloat(b.getFloat());
        }
    }

    private static class CharConstructor extends Fixed4SizeConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.CHAR;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putChar(b.getInt());
        }
    }

    private static class Decimal32Constructor extends Fixed4SizeConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.DECIMAL32;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putDecimal32(new Decimal32(b.getInt()));
        }
    }

    private static class ULongConstructor extends Fixed8SizeConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.ULONG;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putUnsignedLong(UnsignedLong.valueOf(b.getLong()));
        }
    }

    private static class LongConstructor extends Fixed8SizeConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.LONG;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putLong(b.getLong());
        }
    }

    private static class DoubleConstructor extends Fixed8SizeConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.DOUBLE;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putDouble(b.getDouble());
        }
    }

    private static class TimestampConstructor extends Fixed8SizeConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.TIMESTAMP;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putTimestamp(new Date(b.getLong()));
        }
    }

    private static class Decimal64Constructor extends Fixed8SizeConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.DECIMAL64;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putDecimal64(new Decimal64(b.getLong()));
        }
    }

    private static class Decimal128Constructor extends Fixed16SizeConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.DECIMAL128;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putDecimal128(new Decimal128(b.getLong(), b.getLong()));
        }
    }

    private static class UUIDConstructor extends Fixed16SizeConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.UUID;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putUUID(new UUID(b.getLong(), b.getLong()));
        }
    }

    private static abstract class SmallVariableConstructor implements TypeConstructor
    {

        @Override
        public int size(ByteBuffer b)
        {
            int position = b.position();
            if(b.hasRemaining())
            {
                int size = b.get() & 0xff;
                b.position(position);

                return size+1;
            }
            else
            {
                return 1;
            }
        }

    }

    private static abstract class VariableConstructor implements TypeConstructor
    {

        @Override
        public int size(ByteBuffer b)
        {
            int position = b.position();
            if(b.remaining()>=4)
            {
                int size = b.getInt();
                b.position(position);

                return size+4;
            }
            else
            {
                return 4;
            }
        }

    }


    private static class SmallBinaryConstructor extends SmallVariableConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.BINARY;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            int size = b.get() & 0xff;
            byte[] bytes = new byte[size];
            b.get(bytes);
            data.putBinary(bytes);
        }
    }

    private static class SmallSymbolConstructor extends SmallVariableConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.SYMBOL;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            int size = b.get() & 0xff;
            byte[] bytes = new byte[size];
            b.get(bytes);
            data.putSymbol(Symbol.valueOf(new String(bytes, ASCII)));
        }
    }


    private static class SmallStringConstructor extends SmallVariableConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.STRING;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            int size = b.get() & 0xff;
            byte[] bytes = new byte[size];
            b.get(bytes);
            data.putString(new String(bytes, UTF_8));
        }
    }

    private static class BinaryConstructor extends VariableConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.BINARY;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            int size = b.getInt();
            byte[] bytes = new byte[size];
            b.get(bytes);
            data.putBinary(bytes);
        }
    }

    private static class SymbolConstructor extends VariableConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.SYMBOL;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            int size = b.getInt();
            byte[] bytes = new byte[size];
            b.get(bytes);
            data.putSymbol(Symbol.valueOf(new String(bytes, ASCII)));
        }
    }


    private static class StringConstructor extends VariableConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.STRING;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            int size = b.getInt();
            byte[] bytes = new byte[size];
            b.get(bytes);
            data.putString(new String(bytes, UTF_8));
        }
    }


    private static class SmallListConstructor extends SmallVariableConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.LIST;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            int size = b.get() & 0xff;
            ByteBuffer buf = b.slice();
            buf.limit(size);
            b.position(b.position()+size);
            int count = buf.get() & 0xff;
            data.putList();
            parseChildren(data, buf, count);
        }
    }


    private static class SmallMapConstructor extends SmallVariableConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.MAP;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            int size = b.get() & 0xff;
            ByteBuffer buf = b.slice();
            buf.limit(size);
            b.position(b.position()+size);
            int count = buf.get() & 0xff;
            data.putMap();
            parseChildren(data, buf, count);
        }
    }


    private static class ListConstructor extends VariableConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.LIST;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            int size = b.getInt();
            ByteBuffer buf = b.slice();
            buf.limit(size);
            b.position(b.position()+size);
            int count = buf.getInt();
            data.putList();
            parseChildren(data, buf, count);
        }
    }


    private static class MapConstructor extends VariableConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.MAP;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            int size = b.getInt();
            ByteBuffer buf = b.slice();
            buf.limit(size);
            b.position(b.position()+size);
            int count = buf.getInt();
            data.putMap();
            parseChildren(data, buf, count);
        }
    }


    private static void parseChildren(Data data, ByteBuffer buf, int count)
    {
        data.enter();
        for(int i = 0; i < count; i++)
        {
            TypeConstructor c = readConstructor(buf);
            final int size = c.size(buf);
            final int remaining = buf.remaining();
            if(size <= remaining)
            {
                c.parse(buf, data);
            }
            else
            {
                throw new IllegalArgumentException("Malformed data");
            }

        }
        data.exit();
    }

    private static class DescribedTypeConstructor implements TypeConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.DESCRIBED;
        }

        @Override
        public int size(ByteBuffer b)
        {
            ByteBuffer buf = b.slice();
            if(buf.hasRemaining())
            {
                TypeConstructor c = readConstructor(buf);
                int size = c.size(buf);
                if(buf.remaining()>size)
                {
                    buf.position(size + 1);
                    c = readConstructor(buf);
                    return size + 2 + c.size(buf);
                }
                else
                {
                    return size + 2;
                }
            }
            else
            {
                return 1;
            }

        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {
            data.putDescribed();
            data.enter();
            TypeConstructor c = readConstructor(b);
            c.parse(b, data);
            c = readConstructor(b);
            c.parse(b, data);
            data.exit();
        }
    }

    private static class SmallArrayConstructor extends SmallVariableConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.ARRAY;
        }

        @Override
        public void parse(ByteBuffer b, Data data)
        {

            int size = b.get() & 0xff;
            ByteBuffer buf = b.slice();
            buf.limit(size);
            b.position(b.position()+size);
            int count = buf.get() & 0xff;
            parseArray(data, buf, count);
        }

    }

    private static class ArrayConstructor extends VariableConstructor
    {

        @Override
        public Data.DataType getType()
        {
            return Data.DataType.ARRAY;
        }


        @Override
        public void parse(ByteBuffer b, Data data)
        {

            int size = b.getInt();
            ByteBuffer buf = b.slice();
            buf.limit(size);
            b.position(b.position()+size);
            int count = buf.getInt();
            parseArray(data, buf, count);
        }
    }

    private static void parseArray(Data data, ByteBuffer buf, int count)
    {
        byte type = buf.get();
        boolean isDescribed = type == (byte)0x00;
        int descriptorPosition = buf.position();
        if(isDescribed)
        {
            TypeConstructor descriptorTc = readConstructor(buf);
            buf.position(buf.position()+descriptorTc.size(buf));
            type = buf.get();
            if(type == (byte)0x00)
            {
                throw new IllegalArgumentException("Malformed array data");
            }

        }
        TypeConstructor tc = _constructors[type&0xff];

        data.putArray(isDescribed, tc.getType());
        data.enter();
        if(isDescribed)
        {
            int position = buf.position();
            buf.position(descriptorPosition);
            TypeConstructor descriptorTc = readConstructor(buf);
            descriptorTc.parse(buf,data);
            buf.position(position);
        }
        for(int i = 0; i<count; i++)
        {
            tc.parse(buf,data);
        }

        data.exit();
    }

}