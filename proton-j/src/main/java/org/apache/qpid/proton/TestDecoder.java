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

import org.apache.qpid.proton.TestDecoder;
import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.Decimal128;
import org.apache.qpid.proton.amqp.Decimal32;
import org.apache.qpid.proton.amqp.Decimal64;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedByte;
import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.UnsignedShort;
import org.apache.qpid.proton.amqp.messaging.AmqpValue;
import org.apache.qpid.proton.codec.AMQPDefinedTypes;

import org.apache.qpid.proton.codec.DecoderImpl;
import org.apache.qpid.proton.codec.EncoderImpl;

import java.io.IOException;
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

public class TestDecoder {
    private DecoderImpl decoder;
    private EncoderImpl encoder;
    private ByteBuffer buffer;

    TestDecoder(byte[] data) {
        decoder = new DecoderImpl();
	encoder = new EncoderImpl(decoder);
	AMQPDefinedTypes.registerAllTypes(decoder, encoder);
	buffer = ByteBuffer.allocate(data.length);
	buffer.put(data);
	buffer.rewind();
        decoder.setByteBuffer(buffer);
    }

    public Boolean readBoolean() { return decoder.readBoolean(); }
    public Byte readByte() { return decoder.readByte(); }
    public Short readShort() { return decoder.readShort(); }
    public Integer readInteger() { return decoder.readInteger(); }
    public Long readLong() { return decoder.readLong(); }
    public UnsignedByte readUnsignedByte() { return decoder.readUnsignedByte(); }
    public UnsignedShort readUnsignedShort() { return decoder.readUnsignedShort(); }
    public UnsignedInteger readUnsignedInteger() { return decoder.readUnsignedInteger(); }
    public UnsignedLong readUnsignedLong() { return decoder.readUnsignedLong(); }
    public Character readCharacter() { return decoder.readCharacter(); }
    public Float readFloat() { return decoder.readFloat(); }
    public Double readDouble() { return decoder.readDouble(); }
    public UUID readUUID() { return decoder.readUUID(); }
    public Decimal32 readDecimal32() { return decoder.readDecimal32(); }
    public Decimal64 readDecimal64() { return decoder.readDecimal64(); }
    public Decimal128 readDecimal128() { return decoder.readDecimal128(); }
    public Date readTimestamp() { return decoder.readTimestamp(); }
    public Binary readBinary() { return decoder.readBinary(); }
    public Symbol readSymbol() { return decoder.readSymbol(); }
    public String readString() { return decoder.readString(); }
    public List readList() { return decoder.readList(); }
    public Map readMap() { return decoder.readMap(); }
    public Object[] readArray() { return decoder.readArray(); }
    public boolean[] readBooleanArray() { return decoder.readBooleanArray(); }
    public byte[] readByteArray() { return decoder.readByteArray(); }
    public short[] readShortArray() { return decoder.readShortArray(); }
    public int[] readIntegerArray() { return decoder.readIntegerArray(); }
    public long[] readLongArray() { return decoder.readLongArray(); }
    public float[] readFloatArray() { return decoder.readFloatArray(); }
    public double[] readDoubleArray() { return decoder.readDoubleArray(); }
    public char[] readCharacterArray() { return decoder.readCharacterArray(); }
    public Object readObject() { return decoder.readObject(); }
}
