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
    BOOL       => qpid::proton::Mapping->new(
        "bool",
        $cproton_perl::PN_BOOL,
        "put_bool",
        "get_bool"),
    UBYTE      => qpid::proton::Mapping->new(
        "ubyte",
        $cproton_perl::PN_UBYTE,
        "put_ubyte",
        "get_ubyte"),
    BYTE       => qpid::proton::Mapping->new(
        "byte",
        $cproton_perl::PN_BYTE,
        "put_byte",
        "get_byte"),
    USHORT     => qpid::proton::Mapping->new(
        "ushort",
        $cproton_perl::PN_USHORT,
        "put_ushort",
        "get_ushort"),
    SHORT      => qpid::proton::Mapping->new(
        "short",
        $cproton_perl::PN_SHORT,
        "put_short",
        "get_short"),
    UINT       => qpid::proton::Mapping->new(
        "uint",
        $cproton_perl::PN_UINT,
        "put_uint",
        "get_uint"),
    INT        => qpid::proton::Mapping->new(
        "int",
        $cproton_perl::PN_INT,
        "put_int",
        "get_int"),
    CHAR       => qpid::proton::Mapping->new(
        "char",
        $cproton_perl::PN_CHAR,
        "put_char",
        "get_char"),
    ULONG      => qpid::proton::Mapping->new(
        "ulong",
        $cproton_perl::PN_ULONG,
        "put_ulong",
        "get_ulong"),
    LONG       => qpid::proton::Mapping->new(
        "long",
        $cproton_perl::PN_LONG,
        "put_long",
        "get_long"),
    TIMESTAMP  => qpid::proton::Mapping->new(
        "timestamp",
        $cproton_perl::PN_TIMESTAMP,
        "put_timestamp",
        "get_timestamp"),
    FLOAT      => qpid::proton::Mapping->new(
        "float",
        $cproton_perl::PN_FLOAT,
        "put_float",
        "get_float"),
    DOUBLE     => qpid::proton::Mapping->new(
        "double",
        $cproton_perl::PN_DOUBLE,
        "put_double",
        "get_double"),
    DECIMAL32  => qpid::proton::Mapping->new(
        "decimal32",
        $cproton_perl::PN_DECIMAL32,
        "put_decimal32",
        "get_decimal32"),
    DECIMAL64  => qpid::proton::Mapping->new(
        "decimal64",
        $cproton_perl::PN_DECIMAL64,
        "put_decimal64",
        "get_decimal64"),
    DECIMAL128 => qpid::proton::Mapping->new(
        "decimal128",
        $cproton_perl::PN_DECIMAL128,
        "put_decimal128",
        "get_decimal128"),
    UUID       => qpid::proton::Mapping->new(
        "uuid",
        $cproton_perl::PN_UUID,
        "put_uuid",
        "get_uuid"),
    BINARY     => qpid::proton::Mapping->new(
        "binary",
        $cproton_perl::PN_BINARY,
        "put_binary",
        "get_binary"),
    STRING     => qpid::proton::Mapping->new(
        "string",
        $cproton_perl::PN_STRING,
        "put_string",
        "get_string"),
    SYMBOL     => qpid::proton::Mapping->new(
        "symbol",
        $cproton_perl::PN_SYMBOL,
        "put_symbol",
        "get_symbol"),
    ARRAY     => qpid::proton::Mapping->new(
        "array",
        $cproton_perl::PN_ARRAY,
        "put_array",
        "get_array"),
    LIST      => qpid::proton::Mapping->new(
        "list",
        $cproton_perl::PN_LIST,
        "put_list",
        "get_list"),
    MAP      => qpid::proton::Mapping->new(
        "map",
        $cproton_perl::PN_MAP,
        "put_map_helper",
        "get_map_helper"),
};

package qpid::proton::Errors;

use constant {
    NONE => 0,
    EOS => $cproton_perl::PN_EOS,
    ERROR => $cproton_perl::PN_ERR,
    OVERFLOW => $cproton_perl::PN_OVERFLOW,
    UNDERFLOW => $cproton_perl::PN_UNDERFLOW,
    STATE => $cproton_perl::PN_STATE_ERR,
    ARGUMENT => $cproton_perl::PN_ARG_ERR,
    TIMEOUT => $cproton_perl::PN_TIMEOUT,
    INTERRUPTED => $cproton_perl::PN_INTR,
    INPROGRESS => $cproton_perl::PN_INPROGRESS,
};

package qpid::proton::Tracker;

use constant {
    ABORTED => $cproton_perl::PN_STATUS_ABORTED,
    ACCEPTED => $cproton_perl::PN_STATUS_ACCEPTED,
    REJECTED => $cproton_perl::PN_STATUS_REJECTED,
    PENDING => $cproton_perl::PN_STATUS_PENDING,
    SETTLED => $cproton_perl::PN_STATUS_SETTLED,
    UNKNOWN => undef,
};

1;
