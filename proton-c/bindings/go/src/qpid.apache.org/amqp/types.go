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

package amqp

//#include "codec_shim.h"
import "C"

import (
	"bytes"
	"fmt"
	"reflect"
	"time"
	"unsafe"
)

func (t C.pn_type_t) String() string {
	switch C.pn_type_t(t) {
	case C.PN_NULL:
		return "null"
	case C.PN_BOOL:
		return "bool"
	case C.PN_UBYTE:
		return "ubyte"
	case C.PN_BYTE:
		return "byte"
	case C.PN_USHORT:
		return "ushort"
	case C.PN_SHORT:
		return "short"
	case C.PN_CHAR:
		return "char"
	case C.PN_UINT:
		return "uint"
	case C.PN_INT:
		return "int"
	case C.PN_ULONG:
		return "ulong"
	case C.PN_LONG:
		return "long"
	case C.PN_TIMESTAMP:
		return "timestamp"
	case C.PN_FLOAT:
		return "float"
	case C.PN_DOUBLE:
		return "double"
	case C.PN_DECIMAL32:
		return "decimal32"
	case C.PN_DECIMAL64:
		return "decimal64"
	case C.PN_DECIMAL128:
		return "decimal128"
	case C.PN_UUID:
		return "uuid"
	case C.PN_BINARY:
		return "binary"
	case C.PN_STRING:
		return "string"
	case C.PN_SYMBOL:
		return "symbol"
	case C.PN_DESCRIBED:
		return "described"
	case C.PN_ARRAY:
		return "array"
	case C.PN_LIST:
		return "list"
	case C.PN_MAP:
		return "map"
	case C.PN_INVALID:
		return "no-data"
	default:
		return fmt.Sprintf("unknown-type(%d)", t)
	}
}

// Go types
var (
	bytesType = reflect.TypeOf([]byte{})
	valueType = reflect.TypeOf(reflect.Value{})
)

// TODO aconway 2015-04-08: can't handle AMQP maps with key types that are not valid Go map keys.

// Map is a generic map that can have mixed key and value types and so can represent any AMQP map
type Map map[interface{}]interface{}

// List is a generic list that can hold mixed values and can represent any AMQP list.
//
type List []interface{}

// Symbol is a string that is encoded as an AMQP symbol
type Symbol string

func (s Symbol) GoString() string { return fmt.Sprintf("s\"%s\"", s) }

// Binary is a string that is encoded as an AMQP binary.
// It is a string rather than a byte[] because byte[] is not hashable and can't be used as
// a map key, AMQP frequently uses binary types as map keys. It can convert to and from []byte
type Binary string

func (b Binary) GoString() string { return fmt.Sprintf("b\"%s\"", b) }

// GoString for Map prints values with their types, useful for debugging.
func (m Map) GoString() string {
	out := &bytes.Buffer{}
	fmt.Fprintf(out, "%T{", m)
	i := len(m)
	for k, v := range m {
		fmt.Fprintf(out, "%T(%#v): %T(%#v)", k, k, v, v)
		i--
		if i > 0 {
			fmt.Fprint(out, ", ")
		}
	}
	fmt.Fprint(out, "}")
	return out.String()
}

// GoString for List prints values with their types, useful for debugging.
func (l List) GoString() string {
	out := &bytes.Buffer{}
	fmt.Fprintf(out, "%T{", l)
	for i := 0; i < len(l); i++ {
		fmt.Fprintf(out, "%T(%#v)", l[i], l[i])
		if i == len(l)-1 {
			fmt.Fprint(out, ", ")
		}
	}
	fmt.Fprint(out, "}")
	return out.String()
}

// pnTime converts Go time.Time to Proton millisecond Unix time.
func pnTime(t time.Time) C.pn_timestamp_t {
	secs := t.Unix()
	// Note: sub-second accuracy is not guaraunteed if the Unix time in
	// nanoseconds cannot be represented by an int64 (sometime around year 2260)
	msecs := (t.UnixNano() % int64(time.Second)) / int64(time.Millisecond)
	return C.pn_timestamp_t(secs*1000 + msecs)
}

// goTime converts a pn_timestamp_t to a Go time.Time.
func goTime(t C.pn_timestamp_t) time.Time {
	secs := int64(t) / 1000
	nsecs := (int64(t) % 1000) * int64(time.Millisecond)
	return time.Unix(secs, nsecs)
}

func goBytes(cBytes C.pn_bytes_t) (bytes []byte) {
	if cBytes.start != nil {
		bytes = C.GoBytes(unsafe.Pointer(cBytes.start), C.int(cBytes.size))
	}
	return
}

func goString(cBytes C.pn_bytes_t) (str string) {
	if cBytes.start != nil {
		str = C.GoStringN(cBytes.start, C.int(cBytes.size))
	}
	return
}

func pnBytes(b []byte) C.pn_bytes_t {
	if len(b) == 0 {
		return C.pn_bytes_t{0, nil}
	} else {
		return C.pn_bytes_t{C.size_t(len(b)), (*C.char)(unsafe.Pointer(&b[0]))}
	}
}

func cPtr(b []byte) *C.char {
	if len(b) == 0 {
		return nil
	}
	return (*C.char)(unsafe.Pointer(&b[0]))
}

func cLen(b []byte) C.size_t {
	return C.size_t(len(b))
}
