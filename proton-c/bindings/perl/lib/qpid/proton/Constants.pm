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
    BOOL       => $cproton_perl::PN_BOOL,
    UBYTE      => $cproton_perl::PN_UBYTE,
    BYTE       => $cproton_perl::PN_BYTE,
    USHORT     => $cproton_perl::PN_USHORT,
    SHORT      => $cproton_perl::PN_SHORT,
    UINT       => $cproton_perl::PN_UINT,
    INT        => $cproton_perl::PN_INT,
    CHAR       => $cproton_perl::PN_CHAR,
    ULONG      => $cproton_perl::PN_ULONG,
    LONG       => $cproton_perl::PN_LONG,
    TIMESTAMP  => $cproton_perl::PN_TIMESTAMP,
    FLOAT      => $cproton_perl::PN_FLOAT,
    DOUBLE     => $cproton_perl::PN_DOUBLE,
    DECIMAL32  => $cproton_perl::PN_DECIMAL32,
    DECIMAL64  => $cproton_perl::PN_DECIMAL64,
    DECIMAL128 => $cproton_perl::PN_DECIMAL128,
    UUID       => $cproton_perl::PN_UUID,
    BINARY     => $cproton_perl::PN_BINARY,
    STRING     => $cproton_perl::PN_STRING,
    SYMBOL     => $cproton_perl::PN_SYMBOL,
    DESCRIBED  => $cproton_perl::PN_DESCRIBED,
    ARRAY      => $cproton_perl::PN_ARRAY,
    LIST       => $cproton_perl::PN_LIST,
    MAP        => $cproton_perl::PN_MAP,
};



1;
