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

public interface Encoder<B>
{
    void writeNull(B buffer);

    void writeBoolean(B buffer,boolean bool);

    void writeBoolean(B buffer,Boolean bool);

    void writeUnsignedByte(B buffer,UnsignedByte ubyte);

    void writeUnsignedShort(B buffer,UnsignedShort ushort);

    void writeUnsignedInteger(B buffer,UnsignedInteger ushort);

    void writeUnsignedLong(B buffer,UnsignedLong ulong);

    void writeByte(B buffer,byte b);

    void writeByte(B buffer,Byte b);

    void writeShort(B buffer,short s);

    void writeShort(B buffer,Short s);

    void writeInteger(B buffer,int i);

    void writeInteger(B buffer,Integer i);

    void writeLong(B buffer,long l);

    void writeLong(B buffer,Long l);

    void writeFloat(B buffer,float f);

    void writeFloat(B buffer,Float f);

    void writeDouble(B buffer,double d);

    void writeDouble(B buffer,Double d);

    void writeDecimal32(B buffer,Decimal32 d);

    void writeDecimal64(B buffer,Decimal64 d);

    void writeDecimal128(B buffer,Decimal128 d);

    void writeCharacter(B buffer,char c);

    void writeCharacter(B buffer,Character c);

    void writeTimestamp(B buffer,long d);
    void writeTimestamp(B buffer,Date d);

    void writeUUID(B buffer,UUID uuid);

    void writeBinary(B buffer,Binary b);

    void writeString(B buffer,String s);

    void writeSymbol(B buffer,Symbol s);

    void writeList(B buffer,List l);

    void writeMap(B buffer,Map m);

    void writeDescribedType(B buffer,DescribedType d);

    void writeArray(B buffer,boolean[] a);
    void writeArray(B buffer,byte[] a);
    void writeArray(B buffer,short[] a);
    void writeArray(B buffer,int[] a);
    void writeArray(B buffer,long[] a);
    void writeArray(B buffer,float[] a);
    void writeArray(B buffer,double[] a);
    void writeArray(B buffer,char[] a);
    void writeArray(B buffer,Object[] a);

    void writeObject(B buffer,Object o);

    <V> void register(AMQPType<V> type);

    AMQPType getType(Object element);
}
