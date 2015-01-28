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
 * Encodings
 *
 */

public interface Encodings
{

    public static final short NULL = 0x40;
    public static final short BOOLEAN = 0x56;
    public static final short TRUE = 0x41;
    public static final short FALSE = 0x42;
    public static final short UBYTE = 0x50;
    public static final short USHORT = 0x60;
    public static final short UINT = 0x70;
    public static final short SMALLUINT = 0x52;
    public static final short UINT0 = 0x43;
    public static final short ULONG = 0x80;
    public static final short SMALLULONG = 0x53;
    public static final short ULONG0 = 0x44;
    public static final short BYTE = 0x51;
    public static final short SHORT = 0x61;
    public static final short INT = 0x71;
    public static final short SMALLINT = 0x54;
    public static final short LONG = 0x81;
    public static final short SMALLLONG = 0x55;
    public static final short FLOAT = 0x72;
    public static final short DOUBLE = 0x82;
    public static final short DECIMAL32 = 0x74;
    public static final short DECIMAL64 = 0x84;
    public static final short DECIMAL128 = 0x94;
    public static final short UTF32 = 0x73;
    public static final short MS64 = 0x83;
    public static final short UUID = 0x98;
    public static final short VBIN8 = 0xa0;
    public static final short VBIN32 = 0xb0;
    public static final short STR8 = 0xa1;
    public static final short STR32 = 0xb1;
    public static final short SYM8 = 0xa3;
    public static final short SYM32 = 0xb3;
    public static final short LIST0 = 0x45;
    public static final short LIST8 = 0xc0;
    public static final short LIST32 = 0xd0;
    public static final short MAP8 = 0xc1;
    public static final short MAP32 = 0xd1;
    public static final short ARRAY8 = 0xe0;
    public static final short ARRAY32 = 0xf0;

}
