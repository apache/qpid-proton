/*
Licensed to the Apache Software Foundation (ASF) under one
or more contributor license agreements.  See the NOTICE file
distributed with this work for additional information
regarding copyright ownership.  The ASF licenses this file
to you under the Apache License, Version 2.0 (the
"License"); you may not use this file except in compliance
with the License.  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing,
software distributed under the License is distributed on an
"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
KIND, either express or implied.  See the License for the
specific language governing permissions and limitations
under the License.
*/

package proton

// #include <proton/codec.h>
import "C"

import (
	"reflect"
)

type pnGetter func(data *C.pn_data_t) reflect.Value

type pnType struct {
	code   C.pn_type_t
	name   string
	getter pnGetter
}

var pnTypes = map[C.pn_type_t]pnType{
	C.PN_NULL:       {C.PN_NULL, "null", nil},
	C.PN_BOOL:       {C.PN_BOOL, "bool", getPnBool},
	C.PN_UBYTE:      {C.PN_UBYTE, "ubyte", getPnUbyte},
	C.PN_BYTE:       {C.PN_BYTE, "byte", getPnByte},
	C.PN_USHORT:     {C.PN_USHORT, "ushort", getPnUshort},
	C.PN_SHORT:      {C.PN_SHORT, "short", getPnShort},
	C.PN_UINT:       {C.PN_UINT, "uint", getPnUint},
	C.PN_INT:        {C.PN_INT, "int", getPnInt},
	C.PN_CHAR:       {C.PN_CHAR, "char", getPnChar},
	C.PN_ULONG:      {C.PN_ULONG, "ulong", getPnUlong},
	C.PN_LONG:       {C.PN_LONG, "long", getPnLong},
	C.PN_TIMESTAMP:  {C.PN_TIMESTAMP, "timestamp", nil},
	C.PN_FLOAT:      {C.PN_FLOAT, "float", getPnFloat},
	C.PN_DOUBLE:     {C.PN_DOUBLE, "double", getPnDouble},
	C.PN_DECIMAL32:  {C.PN_DECIMAL32, "decimal32", nil},
	C.PN_DECIMAL64:  {C.PN_DECIMAL64, "decimal64", nil},
	C.PN_DECIMAL128: {C.PN_DECIMAL128, "decimal128", nil},
	C.PN_UUID:       {C.PN_UUID, "uuid", nil},
	C.PN_BINARY:     {C.PN_BINARY, "binary", getPnBinary},
	C.PN_STRING:     {C.PN_STRING, "string", getPnString},
	C.PN_SYMBOL:     {C.PN_SYMBOL, "symbol", getPnSymbol},
	C.PN_DESCRIBED:  {C.PN_DESCRIBED, "described", nil},
	C.PN_ARRAY:      {C.PN_ARRAY, "array", nil},
	C.PN_LIST:       {C.PN_LIST, "list", nil},
}

func pnTypeName(t C.pn_type_t) string {
	name := pnTypes[t].name
	return nonBlank(name, "unknown type")
}
