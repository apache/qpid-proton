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
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedByte;
import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.UnsignedShort;

import java.util.Date;
import java.util.List;
import java.util.Map;
import java.util.UUID;

public interface Decoder<B>
{
    public static interface ListProcessor<T>
    {
        T process(int count, Encoder encoder);
    }


    Boolean readBoolean(B buffer);
    Boolean readBoolean(B buffer, Boolean defaultVal);
    boolean readBoolean(B buffer, boolean defaultVal);

    Byte readByte(B buffer);
    Byte readByte(B buffer, Byte defaultVal);
    byte readByte(B buffer, byte defaultVal);

    Short readShort(B buffer);
    Short readShort(B buffer, Short defaultVal);
    short readShort(B buffer, short defaultVal);

    Integer readInteger(B buffer);
    Integer readInteger(B buffer, Integer defaultVal);
    int readInteger(B buffer, int defaultVal);

    Long readLong(B buffer);
    Long readLong(B buffer, Long defaultVal);
    long readLong(B buffer, long defaultVal);

    UnsignedByte readUnsignedByte(B buffer);
    UnsignedByte readUnsignedByte(B buffer, UnsignedByte defaultVal);

    UnsignedShort readUnsignedShort(B buffer);
    UnsignedShort readUnsignedShort(B buffer, UnsignedShort defaultVal);

    UnsignedInteger readUnsignedInteger(B buffer);
    UnsignedInteger readUnsignedInteger(B buffer, UnsignedInteger defaultVal);

    UnsignedLong readUnsignedLong(B buffer);
    UnsignedLong readUnsignedLong(B buffer, UnsignedLong defaultVal);

    Character readCharacter(B buffer);
    Character readCharacter(B buffer, Character defaultVal);
    char readCharacter(B buffer, char defaultVal);

    Float readFloat(B buffer);
    Float readFloat(B buffer, Float defaultVal);
    float readFloat(B buffer, float defaultVal);

    Double readDouble(B buffer);
    Double readDouble(B buffer, Double defaultVal);
    double readDouble(B buffer, double defaultVal);

    UUID readUUID(B buffer);
    UUID readUUID(B buffer, UUID defaultValue);

    Decimal32 readDecimal32(B buffer);
    Decimal32 readDecimal32(B buffer, Decimal32 defaultValue);

    Decimal64 readDecimal64(B buffer);
    Decimal64 readDecimal64(B buffer, Decimal64 defaultValue);

    Decimal128 readDecimal128(B buffer);
    Decimal128 readDecimal128(B buffer, Decimal128 defaultValue);

    Date readTimestamp(B buffer);
    Date readTimestamp(B buffer, Date defaultValue);

    Binary readBinary(B buffer);
    Binary readBinary(B buffer, Binary defaultValue);

    Symbol readSymbol(B buffer);
    Symbol readSymbol(B buffer, Symbol defaultValue);

    String readString(B buffer);
    String readString(B buffer, String defaultValue);

    List readList(B buffer);
    <T> void readList(B buffer, ListProcessor<T> processor);

    Map readMap(B buffer);

    <T> T[] readArray(B buffer, Class<T> clazz);

    Object[] readArray(B buffer);

    boolean[] readBooleanArray(B buffer);
    byte[] readByteArray(B buffer);
    short[] readShortArray(B buffer);
    int[] readIntegerArray(B buffer);
    long[] readLongArray(B buffer);
    float[] readFloatArray(B buffer);
    double[] readDoubleArray(B buffer);
    char[] readCharacterArray(B buffer);

    <T> T[] readMultiple(B buffer, Class<T> clazz);

    Object[] readMultiple(B buffer);
    byte[] readByteMultiple(B buffer);
    short[] readShortMultiple(B buffer);
    int[] readIntegerMultiple(B buffer);
    long[] readLongMultiple(B buffer);
    float[] readFloatMultiple(B buffer);
    double[] readDoubleMultiple(B buffer);
    char[] readCharacterMultiple(B buffer);

    Object readObject(B buffer);
    Object readObject(B buffer, Object defaultValue);

    void register(final Object descriptor, final DescribedTypeConstructor dtc);


}
