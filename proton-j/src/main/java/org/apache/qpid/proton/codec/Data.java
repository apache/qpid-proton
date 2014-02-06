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

import java.math.BigDecimal;
import java.nio.ByteBuffer;
import java.util.Date;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.Decimal128;
import org.apache.qpid.proton.amqp.Decimal32;
import org.apache.qpid.proton.amqp.Decimal64;
import org.apache.qpid.proton.amqp.DescribedType;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedByte;
import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.UnsignedShort;

public interface Data
{


    enum DataType
    {
        NULL,
        BOOL,
        UBYTE,
        BYTE,
        USHORT,
        SHORT,
        UINT,
        INT,
        CHAR,
        ULONG,
        LONG,
        TIMESTAMP,
        FLOAT,
        DOUBLE,
        DECIMAL32,
        DECIMAL64,
        DECIMAL128,
        UUID,
        BINARY,
        STRING,
        SYMBOL,
        DESCRIBED,
        ARRAY,
        LIST,
        MAP
    }

    void free();
//    int errno();
//    String error();

    void clear();
    long size();
    void rewind();
    DataType next();
    DataType prev();
    boolean enter();
    boolean exit();
    boolean lookup(String name);

    DataType type();

//    int print();
//    int format(ByteBuffer buf);
    Binary encode();
    long encode(ByteBuffer buf);
    long decode(ByteBuffer buf);

    void putList();
    void putMap();
    void putArray(boolean described, DataType type);
    void putDescribed();
    void putNull();
    void putBoolean(boolean b);
    void putUnsignedByte(UnsignedByte ub);
    void putByte(byte b);
    void putUnsignedShort(UnsignedShort us);
    void putShort(short s);
    void putUnsignedInteger(UnsignedInteger ui);
    void putInt(int i);
    void putChar(int c);
    void putUnsignedLong(UnsignedLong ul);
    void putLong(long l);
    void putTimestamp(Date t);
    void putFloat(float f);
    void putDouble(double d);
    void putDecimal32(Decimal32 d);
    void putDecimal64(Decimal64 d);
    void putDecimal128(Decimal128 d);
    void putUUID(UUID u);
    void putBinary(Binary bytes);
    void putBinary(byte[] bytes);
    void putString(String string);
    void putSymbol(Symbol symbol);
    void putObject(Object o);
    void putJavaMap(Map<Object, Object> map);
    void putJavaList(List<Object> list);
    void putJavaArray(Object[] array);
    void putDescribedType(DescribedType dt);

    long getList();
    long getMap();
    long getArray();
    boolean isArrayDescribed();
    DataType getArrayType();
    boolean isDescribed();
    boolean isNull();
    boolean getBoolean();
    UnsignedByte getUnsignedByte();
    byte getByte();
    UnsignedShort getUnsignedShort();
    short getShort();
    UnsignedInteger getUnsignedInteger();
    int getInt();
    int getChar();
    UnsignedLong getUnsignedLong();
    long getLong();
    Date getTimestamp();
    float getFloat();
    double getDouble();
    Decimal32 getDecimal32();
    Decimal64 getDecimal64();
    Decimal128 getDecimal128();
    UUID getUUID();
    Binary getBinary();
    String getString();
    Symbol getSymbol();
    Object getObject();
    Map<Object, Object> getJavaMap();
    List<Object> getJavaList();
    Object[] getJavaArray();
    DescribedType getDescribedType();
    //pnAtomT getAtom();

    void copy(Data src);
    void append(Data src);
    void appendn(Data src, int limit);
    void narrow();
    void widen();

    String format();

    // void dump();


}
