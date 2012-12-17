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

public interface EncodingCodes
{
    public static final byte DESCRIBED_TYPE_INDICATOR = (byte) 0x00;

    public static final byte NULL                     = (byte) 0x40;

    public static final byte BOOLEAN                  = (byte) 0x56;
    public static final byte BOOLEAN_TRUE             = (byte) 0x41;
    public static final byte BOOLEAN_FALSE            = (byte) 0x42;

    public static final byte UBYTE                    = (byte) 0x50;

    public static final byte USHORT                   = (byte) 0x60;

    public static final byte UINT                     = (byte) 0x70;
    public static final byte SMALLUINT                = (byte) 0x52;
    public static final byte UINT0                    = (byte) 0x43;

    public static final byte ULONG                    = (byte) 0x80;
    public static final byte SMALLULONG               = (byte) 0x53;
    public static final byte ULONG0                   = (byte) 0x44;

    public static final byte BYTE                     = (byte) 0x51;

    public static final byte SHORT                    = (byte) 0x61;

    public static final byte INT                      = (byte) 0x71;
    public static final byte SMALLINT                 = (byte) 0x54;

    public static final byte LONG                     = (byte) 0x81;
    public static final byte SMALLLONG                = (byte) 0x55;

    public static final byte FLOAT                    = (byte) 0x72;

    public static final byte DOUBLE                   = (byte) 0x82;

    public static final byte DECIMAL32                = (byte) 0x74;

    public static final byte DECIMAL64                = (byte) 0x84;

    public static final byte DECIMAL128               = (byte) 0x94;

    public static final byte CHAR                     = (byte) 0x73;

    public static final byte TIMESTAMP                = (byte) 0x83;

    public static final byte UUID                     = (byte) 0x98;

    public static final byte VBIN8                    = (byte) 0xa0;
    public static final byte VBIN32                   = (byte) 0xb0;

    public static final byte STR8                     = (byte) 0xa1;
    public static final byte STR32                    = (byte) 0xb1;

    public static final byte SYM8                     = (byte) 0xa3;
    public static final byte SYM32                    = (byte) 0xb3;

    public static final byte LIST0                    = (byte) 0x45;
    public static final byte LIST8                    = (byte) 0xc0;
    public static final byte LIST32                   = (byte) 0xd0;

    public static final byte MAP8                     = (byte) 0xc1;
    public static final byte MAP32                    = (byte) 0xd1;

    public static final byte ARRAY8                   = (byte) 0xe0;
    public static final byte ARRAY32                  = (byte) 0xf0;

}
