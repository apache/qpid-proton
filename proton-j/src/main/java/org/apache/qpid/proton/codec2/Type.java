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
 * Type
 *
 */

public enum Type
{

    NULL,
    BOOLEAN,
    UBYTE,
    USHORT,
    UINT,
    ULONG,
    BYTE,
    SHORT,
    INT,
    LONG,
    FLOAT,
    DOUBLE,
    DECIMAL32,
    DECIMAL64,
    DECIMAL128,
    CHAR,
    UTF32,
    TIMESTAMP,
    UUID,
    BINARY,
    STRING,
    SYMBOL,
    LIST,
    MAP,
    ARRAY;

    public static Type typeOf(int encoding) {
        switch (encoding) {
        case Encodings.NULL:
            return NULL;
        case Encodings.BOOLEAN:
        case Encodings.TRUE:
        case Encodings.FALSE:
            return BOOLEAN;
        case Encodings.UBYTE:
            return UBYTE;
        case Encodings.USHORT:
            return USHORT;
        case Encodings.UINT:
        case Encodings.SMALLUINT:
        case Encodings.UINT0:
            return UINT;
        case Encodings.ULONG:
        case Encodings.SMALLULONG:
        case Encodings.ULONG0:
            return ULONG;
        case Encodings.BYTE:
            return BYTE;
        case Encodings.SHORT:
            return SHORT;
        case Encodings.INT:
        case Encodings.SMALLINT:
            return INT;
        case Encodings.LONG:
        case Encodings.SMALLLONG:
            return LONG;
        case Encodings.FLOAT:
            return FLOAT;
        case Encodings.DOUBLE:
            return DOUBLE;
        case Encodings.DECIMAL32:
            return DECIMAL32;
        case Encodings.DECIMAL64:
            return DECIMAL64;
        case Encodings.DECIMAL128:
            return DECIMAL128;
        case Encodings.UTF32:
            return CHAR;
        case Encodings.MS64:
            return TIMESTAMP;
        case Encodings.UUID:
            return UUID;
        case Encodings.VBIN8:
        case Encodings.VBIN32:
            return BINARY;
        case Encodings.STR8:
        case Encodings.STR32:
            return STRING;
        case Encodings.SYM8:
        case Encodings.SYM32:
            return SYMBOL;
        case Encodings.LIST0:
        case Encodings.LIST8:
        case Encodings.LIST32:
            return LIST;
        case Encodings.MAP8:
        case Encodings.MAP32:
            return MAP;
        case Encodings.ARRAY8:
        case Encodings.ARRAY32:
            return ARRAY;
        default:
            throw new IllegalArgumentException("unrecognized encoding: " + encoding);
        }
    }

}
