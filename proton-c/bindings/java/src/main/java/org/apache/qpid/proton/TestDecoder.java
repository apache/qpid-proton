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

import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.Decimal128;
import org.apache.qpid.proton.amqp.Decimal32;
import org.apache.qpid.proton.amqp.Decimal64;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedByte;
import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.UnsignedShort;
import org.apache.qpid.proton.codec.Data;

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
    private Data data;

    TestDecoder(byte[] encoded) {
	data = Proton.data(encoded.length);
	int offset = 0;
	while (offset < encoded.length) {
	    ByteBuffer buffer = ByteBuffer.wrap(encoded, offset, encoded.length-offset);
	    offset += data.decode(buffer);
	}
	data.rewind();
    }

    public Boolean readBoolean() { data.next(); return data.getBoolean(); }
    public Byte readByte() { data.next(); return data.getByte(); }
    public Short readShort() { data.next(); return data.getShort(); }
    public Integer readInteger() { data.next(); return data.getInt(); }
    public Long readLong() { data.next(); return data.getLong(); }
    public UnsignedByte readUnsignedByte() { data.next(); return data.getUnsignedByte(); }
    public UnsignedShort readUnsignedShort() { data.next(); return data.getUnsignedShort(); }
    public UnsignedInteger readUnsignedInteger() { data.next(); return data.getUnsignedInteger(); }
    public UnsignedLong readUnsignedLong() { data.next(); return data.getUnsignedLong(); }
    public Character readCharacter() { data.next(); return new Character((char)data.getChar()); }
    public Float readFloat() { data.next(); return data.getFloat(); }
    public Double readDouble() { data.next(); return data.getDouble(); }
    public UUID readUUID() { data.next(); return data.getUUID(); }
    public Decimal32 readDecimal32() { data.next(); return data.getDecimal32(); }
    public Decimal64 readDecimal64() { data.next(); return data.getDecimal64(); }
    public Decimal128 readDecimal128() { data.next(); return data.getDecimal128(); }
    public Date readTimestamp() { data.next(); return data.getTimestamp(); }
    public Binary readBinary() { data.next(); return data.getBinary(); }
    public Symbol readSymbol() { data.next(); return data.getSymbol(); }
    public String readString() { data.next(); return data.getString(); }

    // FIXME aconway 2013-02-16:
    public List readList() { data.next(); return data.getJavaList(); }
    public Object[] readArray() { data.next(); return data.getJavaArray(); }
    public Map<Object, Object> readMap() { data.next(); return data.getJavaMap(); }

     public Object readObject() { data.next(); return data.getObject(); }
}
