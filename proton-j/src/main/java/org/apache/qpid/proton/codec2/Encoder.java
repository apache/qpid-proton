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


/**
 * Encoder
 *
 */

public interface Encoder
{

    void putNull();

    void putBoolean(boolean b);

    void putByte(byte b);

    void putShort(short s);

    void putInt(int i);

    void putLong(long l);

    void putUbyte(byte b);

    void putUshort(short s);

    void putUint(int i);

    void putUlong(long l);

    void putFloat(float f);

    void putDouble(double d);

    void putChar(char c);

    void putChar(int utf32);

    void putTimestamp(long t);

    void putUUID(long hi, long lo);

    void putString(String s);

    void putString(byte[] utf8, int offset, int size);

    void putBinary(byte[] bytes, int offset, int size);

    void putSymbol(String s);

    void putSymbol(byte[] ascii, int offset, int size);

    void putList();

    void putMap();

    void putArray(Type t);

    void putDescriptor();

    void end();

}
