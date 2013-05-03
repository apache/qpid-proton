#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

package qpid::proton;

use constant {
    NULL       => $cproton_perl::PN_NULL,
    BOOL       => qpid::proton::TypeHelper->new(
        $cproton_perl::PN_BOOL,
        "put_bool",
        "get_bool"),
    UBYTE      => qpid::proton::TypeHelper->new(
        $cproton_perl::PN_UBYTE,
        "put_ubyte",
        "get_ubyte"),
    BYTE       => qpid::proton::TypeHelper->new(
        $cproton_perl::PN_BYTE,
        "put_byte",
        "get_byte"),
    USHORT     => qpid::proton::TypeHelper->new(
        $cproton_perl::PN_USHORT,
        "put_ushort",
        "get_ushort"),
    SHORT      => qpid::proton::TypeHelper->new(
        $cproton_perl::PN_SHORT,
        "put_short",
        "get_short"),
    UINT       => qpid::proton::TypeHelper->new(
        $cproton_perl::PN_UINT,
        "put_uint",
        "get_uint"),
    INT        => qpid::proton::TypeHelper->new(
        $cproton_perl::PN_INT,
        "put_int",
        "get_int"),
    CHAR       => qpid::proton::TypeHelper->new(
        $cproton_perl::PN_CHAR,
        "put_char",
        "get_char"),
    ULONG      => qpid::proton::TypeHelper->new(
        $cproton_perl::PN_ULONG,
        "put_ulong",
        "get_ulong"),
    LONG       => qpid::proton::TypeHelper->new(
        $cproton_perl::PN_LONG,
        "put_long",
        "get_long"),
    TIMESTAMP  => qpid::proton::TypeHelper->new(
        $cproton_perl::PN_TIMESTAMP,
        "put_timestamp",
        "get_timestamp"),
    FLOAT      => qpid::proton::TypeHelper->new(
        $cproton_perl::PN_FLOAT,
        "put_float",
        "get_float"),
    DOUBLE     => qpid::proton::TypeHelper->new(
        $cproton_perl::PN_DOUBLE,
        "put_double",
        "get_double"),
    DECIMAL32  => qpid::proton::TypeHelper->new(
        $cproton_perl::PN_DECIMAL32,
        "put_decimal32",
        "get_decimal32"),
    DECIMAL64  => qpid::proton::TypeHelper->new(
        $cproton_perl::PN_DECIMAL64,
        "put_decimal64",
        "get_decimal64"),
    DECIMAL128 => qpid::proton::TypeHelper->new(
        $cproton_perl::PN_DECIMAL128,
        "put_decimal128",
        "get_decimal128"),
    UUID       => qpid::proton::TypeHelper->new(
        $cproton_perl::PN_UUID,
        "put_uuid",
        "get_uuid"),
    BINARY     => qpid::proton::TypeHelper->new(
        $cproton_perl::PN_BINARY,
        "put_binary",
        "get_binary"),
    STRING     => qpid::proton::TypeHelper->new(
        $cproton_perl::PN_STRING,
        "put_string",
        "get_string"),
    SYMBOL     => qpid::proton::TypeHelper->new(
        $cproton_perl::PN_SYMBOL,
        "put_symbol",
        "get_symbol")
};

1;
