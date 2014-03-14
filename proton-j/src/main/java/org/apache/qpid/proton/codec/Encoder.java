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

import java.util.Date;
import java.util.List;
import java.util.Map;
import java.util.UUID;

public interface Encoder
{
    void writeNull();

    void writeBoolean(boolean bool);

    void writeBoolean(Boolean bool);

    void writeUnsignedByte(UnsignedByte ubyte);

    void writeUnsignedShort(UnsignedShort ushort);

    void writeUnsignedInteger(UnsignedInteger ushort);

    void writeUnsignedLong(UnsignedLong ulong);

    void writeByte(byte b);

    void writeByte(Byte b);

    void writeShort(short s);

    void writeShort(Short s);

    void writeInteger(int i);

    void writeInteger(Integer i);

    void writeLong(long l);

    void writeLong(Long l);

    void writeFloat(float f);

    void writeFloat(Float f);

    void writeDouble(double d);

    void writeDouble(Double d);

    void writeDecimal32(Decimal32 d);

    void writeDecimal64(Decimal64 d);

    void writeDecimal128(Decimal128 d);

    void writeCharacter(char c);

    void writeCharacter(Character c);

    void writeTimestamp(long d);
    void writeTimestamp(Date d);

    void writeUUID(UUID uuid);

    void writeBinary(Binary b);

    void writeString(String s);

    void writeSymbol(Symbol s);

    void writeList(List l);

    void writeMap(Map m);

    void writeDescribedType(DescribedType d);

    void writeArray(boolean[] a);
    void writeArray(byte[] a);
    void writeArray(short[] a);
    void writeArray(int[] a);
    void writeArray(long[] a);
    void writeArray(float[] a);
    void writeArray(double[] a);
    void writeArray(char[] a);
    void writeArray(Object[] a);

    void writeObject(Object o);

    <V> void register(AMQPType<V> type);

    AMQPType getType(Object element);
}
