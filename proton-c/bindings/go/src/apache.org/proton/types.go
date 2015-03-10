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

type pnGet func(data *C.pn_data_t) reflect.Value
type pnPut func(data *C.pn_data_t, v interface{})

// Information about a proton type
type pnType struct {
	code C.pn_type_t // AMQP type code
	name string      // AMQP type name

	// unmarshal data
	get     pnGet        // pn_data_*_get function wrapper returning a reflect.Value
	getType reflect.Type // go type for unmarshaling into a Value

	// marshal data
	put pnPut // pn_data_*_put function wrapper taking an interface{}
}

func (pt *pnType) String() string { return pt.name }

// pnType definitions for each proton type.
var (
	pnNull       = pnType{code: C.PN_NULL, name: "null"}
	pnBool       = pnType{code: C.PN_BOOL, name: "bool"}
	pnUbyte      = pnType{code: C.PN_UBYTE, name: "ubyte"}
	pnByte       = pnType{code: C.PN_BYTE, name: "byte"}
	pnUshort     = pnType{code: C.PN_USHORT, name: "ushort"}
	pnShort      = pnType{code: C.PN_SHORT, name: "short"}
	pnChar       = pnType{code: C.PN_CHAR, name: "char"}
	pnUint       = pnType{code: C.PN_UINT, name: "uint"}
	pnInt        = pnType{code: C.PN_INT, name: "int"}
	pnUlong      = pnType{code: C.PN_ULONG, name: "ulong"}
	pnLong       = pnType{code: C.PN_LONG, name: "long"}
	pnTimestamp  = pnType{code: C.PN_TIMESTAMP, name: "timestamp"}
	pnFloat      = pnType{code: C.PN_FLOAT, name: "float"}
	pnDouble     = pnType{code: C.PN_DOUBLE, name: "double"}
	pnDecimal32  = pnType{code: C.PN_DECIMAL32, name: "decimal32"}
	pnDecimal64  = pnType{code: C.PN_DECIMAL64, name: "decimal64"}
	pnDecimal128 = pnType{code: C.PN_DECIMAL128, name: "decimal128"}
	pnUuid       = pnType{code: C.PN_UUID, name: "uuid"}
	pnBinary     = pnType{code: C.PN_BINARY, name: "binary"}
	pnString     = pnType{code: C.PN_STRING, name: "string"}
	pnSymbol     = pnType{code: C.PN_SYMBOL, name: "symbol"}
	pnDescribed  = pnType{code: C.PN_DESCRIBED, name: "described"}
	pnArray      = pnType{code: C.PN_ARRAY, name: "array"}
	pnList       = pnType{code: C.PN_LIST, name: "list"}
	pnMap        = pnType{code: C.PN_MAP, name: "map"}
)

// Map from pn_type_t codes to pnType structs
var pnTypes = map[C.pn_type_t]*pnType{
	C.PN_NULL:       &pnNull,
	C.PN_BOOL:       &pnBool,
	C.PN_UBYTE:      &pnUbyte,
	C.PN_BYTE:       &pnByte,
	C.PN_USHORT:     &pnUshort,
	C.PN_SHORT:      &pnShort,
	C.PN_UINT:       &pnUint,
	C.PN_INT:        &pnInt,
	C.PN_CHAR:       &pnChar,
	C.PN_ULONG:      &pnUlong,
	C.PN_LONG:       &pnLong,
	C.PN_TIMESTAMP:  &pnTimestamp,
	C.PN_FLOAT:      &pnFloat,
	C.PN_DOUBLE:     &pnDouble,
	C.PN_DECIMAL32:  &pnDecimal32,
	C.PN_DECIMAL64:  &pnDecimal64,
	C.PN_DECIMAL128: &pnDecimal128,
	C.PN_UUID:       &pnUuid,
	C.PN_BINARY:     &pnBinary,
	C.PN_STRING:     &pnString,
	C.PN_SYMBOL:     &pnSymbol,
	C.PN_DESCRIBED:  &pnDescribed,
	C.PN_ARRAY:      &pnArray,
	C.PN_LIST:       &pnList,
	C.PN_MAP:        &pnMap,
}

// Get the pnType for a pn_type_t code, panic if unknown code.
func getPnType(code C.pn_type_t) *pnType {
	pt, ok := pnTypes[code]
	if !ok {
		panic(errorf("unknown AMQP type code %v", code))
	}
	return pt
}

// Go types
var (
	bytesType = reflect.TypeOf([]byte{})
	valueType = reflect.TypeOf(reflect.Value{})
)
