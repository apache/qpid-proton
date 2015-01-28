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
 * Widths
 *
 */

public interface Widths
{

    public static final short NULL = 0;
    public static final short BOOLEAN = 1;
    public static final short TRUE = 0;
    public static final short FALSE = 0;
    public static final short UBYTE = 1;
    public static final short USHORT = 2;
    public static final short UINT = 4;
    public static final short SMALLUINT = 1;
    public static final short UINT0 = 0;
    public static final short ULONG = 8;
    public static final short SMALLULONG = 1;
    public static final short ULONG0 = 0;
    public static final short BYTE = 1;
    public static final short SHORT = 2;
    public static final short INT = 4;
    public static final short SMALLINT = 1;
    public static final short LONG = 8;
    public static final short SMALLLONG = 1;
    public static final short FLOAT = 4;
    public static final short DOUBLE = 8;
    public static final short DECIMAL32 = 4;
    public static final short DECIMAL64 = 8;
    public static final short DECIMAL128 = 16;
    public static final short UTF32 = 4;
    public static final short MS64 = 8;
    public static final short UUID = 16;
    public static final short VBIN8 = 1;
    public static final short VBIN32 = 4;
    public static final short STR8 = 1;
    public static final short STR32 = 4;
    public static final short SYM8 = 1;
    public static final short SYM32 = 4;
    public static final short LIST0 = 0;
    public static final short LIST8 = 2;
    public static final short LIST32 = 8;
    public static final short MAP8 = 2;
    public static final short MAP32 = 8;
    public static final short ARRAY8 = 2;
    public static final short ARRAY32 = 8;

}
