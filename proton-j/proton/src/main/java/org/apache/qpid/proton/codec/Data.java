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

import org.apache.qpid.proton.type.Binary;
import org.apache.qpid.proton.type.Symbol;
import org.apache.qpid.proton.type.UnsignedByte;
import org.apache.qpid.proton.type.UnsignedInteger;
import org.apache.qpid.proton.type.UnsignedLong;
import org.apache.qpid.proton.type.UnsignedShort;

public interface Data
{
    enum Type
    {
        NULL,
        BOOL,
        UBYTE,
        BYTE,
        USHORT,
        SHORT,
        UINT,
        INT,
        ULONG,
        LONG,
        FLOAT,
        DOUBLE,
        BINARY,
        STRING,
        SYMBOL,
        DESCRIPTOR,
        ARRAY,
        LIST,
        MAP,
        TYPE
    }
    
    Type NULL = Type.NULL;
    Type BOOL = Type.BOOL;
    Type UBYTE = Type.UBYTE;
    Type BYTE = Type.BYTE;
    Type USHORT = Type.USHORT;
    Type SHORT = Type.SHORT;
    Type UINT = Type.UINT;
    Type INT = Type.INT;
    Type ULONG = Type.ULONG;
    Type LONG = Type.LONG;
    Type FLOAT = Type.FLOAT;
    Type DOUBLE = Type.DOUBLE;
    Type BINARY = Type.BINARY;
    Type STRING = Type.STRING;
    Type SYMBOL = Type.SYMBOL;
    Type DESCRIPTOR = Type.DESCRIPTOR;
    Type ARRAY = Type.ARRAY;
    Type LIST = Type.LIST;
    Type MAP = Type.MAP;
    Type TYPE = Type.TYPE;

    void putBool(boolean b);
    void putByte(byte b);
    void putUbyte(UnsignedByte b);
    void putUbyte(short s);
    void putShort(short s);
    void putUshort(UnsignedShort s);
    void putUshort(int i);
    void putInt(int i);
    void putUint(UnsignedInteger i);
    void putUint(long l);
    void putLong(long l);
    void putUlong(UnsignedLong l);
    void putUlong(long l);
    void putFloat(float f);
    void putDouble(double d);
    void putBinary(Binary b);
    void putBinary(byte[] b);
    void putString(String s);
    void putSymbol(Symbol s);
    void putSymbol(String s);
    void putDescriptor();
    void putArray();
    void putList();
    void putType();
}
