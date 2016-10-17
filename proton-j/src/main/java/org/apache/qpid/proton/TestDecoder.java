/*
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
 */
package org.apache.qpid.proton;

import java.nio.ByteBuffer;
import java.util.Date;
import java.util.List;
import java.util.Map;
import java.util.UUID;

import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.Decimal128;
import org.apache.qpid.proton.amqp.Decimal32;
import org.apache.qpid.proton.amqp.Decimal64;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedByte;
import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.UnsignedShort;
import org.apache.qpid.proton.codec.AMQPDefinedTypes;
import org.apache.qpid.proton.codec.DecoderFactory;
import org.apache.qpid.proton.codec.DecoderImpl;
import org.apache.qpid.proton.codec.EncoderImpl;
import org.apache.qpid.proton.codec.ReadableBuffer;

import java.nio.ByteBuffer;
import java.util.Date;
import java.util.List;
import java.util.Map;
import java.util.UUID;


/**
 * TestDecoder is a stripped-down decoder used by interop tests to decode AMQP
 * data.  The native version wraps a DecoderImpl, while the JNI version wraps a
 * Data object.
 */

public class TestDecoder
{
    private DecoderImpl decoder;
    private EncoderImpl encoder;
    private ReadableBuffer buffer;

    TestDecoder(byte[] data)
    {
        decoder = DecoderFactory.getSingleton().getDecoder();
        encoder = DecoderFactory.getSingleton().getEncoder();


        buffer = new ReadableBuffer.ByteBufferReader(ByteBuffer.wrap(data));
    }

    public Boolean readBoolean()
    {
        return decoder.readBoolean(buffer);
    }

    public Byte readByte()
    {
        return decoder.readByte(buffer);
    }

    public Short readShort()
    {
        return decoder.readShort(buffer);
    }

    public Integer readInteger()
    {
        return decoder.readInteger(buffer);
    }

    public Long readLong()
    {
        return decoder.readLong(buffer);
    }

    public UnsignedByte readUnsignedByte()
    {
        return decoder.readUnsignedByte(buffer);
    }

    public UnsignedShort readUnsignedShort()
    {
        return decoder.readUnsignedShort(buffer);
    }

    public UnsignedInteger readUnsignedInteger()
    {
        return decoder.readUnsignedInteger(buffer);
    }

    public UnsignedLong readUnsignedLong()
    {
        return decoder.readUnsignedLong(buffer);
    }

    public Character readCharacter()
    {
        return decoder.readCharacter(buffer);
    }

    public Float readFloat()
    {
        return decoder.readFloat(buffer);
    }

    public Double readDouble()
    {
        return decoder.readDouble(buffer);
    }

    public UUID readUUID()
    {
        return decoder.readUUID(buffer);
    }

    public Decimal32 readDecimal32()
    {
        return decoder.readDecimal32(buffer);
    }

    public Decimal64 readDecimal64()
    {
        return decoder.readDecimal64(buffer);
    }

    public Decimal128 readDecimal128()
    {
        return decoder.readDecimal128(buffer);
    }

    public Date readTimestamp()
    {
        return decoder.readTimestamp(buffer);
    }

    public Binary readBinary()
    {
        return decoder.readBinary(buffer);
    }

    public Symbol readSymbol()
    {
        return decoder.readSymbol(buffer);
    }

    public String readString()
    {
        return decoder.readString(buffer);
    }

    public List readList()
    {
        return decoder.readList(buffer);
    }

    public Map readMap()
    {
        return decoder.readMap(buffer);
    }

    public Object[] readArray()
    {
        return decoder.readArray(buffer);
    }

    public boolean[] readBooleanArray()
    {
        return decoder.readBooleanArray(buffer);
    }

    public byte[] readByteArray()
    {
        return decoder.readByteArray(buffer);
    }

    public short[] readShortArray()
    {
        return decoder.readShortArray(buffer);
    }

    public int[] readIntegerArray()
    {
        return decoder.readIntegerArray(buffer);
    }

    public long[] readLongArray()
    {
        return decoder.readLongArray(buffer);
    }

    public float[] readFloatArray()
    {
        return decoder.readFloatArray(buffer);
    }

    public double[] readDoubleArray()
    {
        return decoder.readDoubleArray(buffer);
    }

    public char[] readCharacterArray()
    {
        return decoder.readCharacterArray(buffer);
    }

    public Object readObject()
    {
        return decoder.readObject(buffer);
    }
}
