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

// #include <proton/codec.h>
import "C"

import (
	"bytes"
	"fmt"
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
	default:
		return fmt.Sprintf("<bad-type %v>", int(t))
	}
}

// The AMQP map type. A generic map that can have mixed-type keys and values.
type Map map[interface{}]interface{}

// The most general AMQP map type, for unusual interoperability cases.
//
// This is not a Go Map but a sequence of {key, value} pairs.
//
// An AnyMap lets you control or examine the encoded ordering of key,value pairs
// and use key values that are not legal as Go map keys.
//
// The amqp.Map, or plain Go map types are easier to use for most cases.
type AnyMap []KeyValue

// Return a Map constructed from an AnyMap.
// Panic if the AnyMap has key values that are not valid Go map keys (e.g. maps, slices)
func (a AnyMap) Map() (m Map) {
	for _, kv := range a {
		m[kv.Key] = kv.Value
	}
	return
}

// KeyValue pair, used by AnyMap
type KeyValue struct{ Key, Value interface{} }

// The AMQP list type. A generic list that can hold mixed-type values.
type List []interface{}

// The generic AMQP array type, used to unmarshal an array with nested array,
// map or list elements. Arrays of simple type T unmarshal to []T
type Array []interface{}

// Symbol is a string that is encoded as an AMQP symbol
type Symbol string

func (s Symbol) String() string   { return string(s) }
func (s Symbol) GoString() string { return fmt.Sprintf("s\"%s\"", s) }

// Binary is a string that is encoded as an AMQP binary.
// It is a string rather than a byte[] because byte[] is not hashable and can't be used as
// a map key, AMQP frequently uses binary types as map keys. It can convert to and from []byte
type Binary string

func (b Binary) String() string   { return string(b) }
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
// Take care to convert zero values to zero values.
func pnTime(t time.Time) C.pn_timestamp_t {
	if t.IsZero() {
		return C.pn_timestamp_t(0)
	}
	return C.pn_timestamp_t(t.UnixNano() / int64(time.Millisecond))
}

// goTime converts a pn_timestamp_t to a Go time.Time.
// Take care to convert zero values to zero values.
func goTime(t C.pn_timestamp_t) time.Time {
	if t == 0 {
		return time.Time{}
	}
	return time.Unix(0, int64(t)*int64(time.Millisecond))
}

func pnDuration(d time.Duration) C.pn_millis_t {
	return (C.pn_millis_t)(d / (time.Millisecond))
}

func goDuration(d C.pn_millis_t) time.Duration {
	return time.Duration(d) * time.Millisecond
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

// AnnotationKey is used as a map key for AMQP annotation maps which are
// allowed to have keys that are either symbol or ulong but no other type.
//
type AnnotationKey struct {
	value interface{}
}

func AnnotationKeySymbol(v Symbol) AnnotationKey { return AnnotationKey{v} }
func AnnotationKeyUint64(v uint64) AnnotationKey { return AnnotationKey{v} }
func AnnotationKeyString(v string) AnnotationKey { return AnnotationKey{Symbol(v)} }

// Returns the value which must be Symbol, uint64 or nil
func (k AnnotationKey) Get() interface{} { return k.value }

func (k AnnotationKey) String() string { return fmt.Sprintf("%v", k.Get()) }

// Described represents an AMQP described type, which is really
// just a pair of AMQP values - the first is treated as a "descriptor",
// and is normally a string or ulong providing information about the type.
// The second is the "value" and can be any AMQP value.
type Described struct {
	Descriptor interface{}
	Value      interface{}
}

// UUID is an AMQP 128-bit Universally Unique Identifier, as defined by RFC-4122 section 4.1.2
type UUID [16]byte

func (u UUID) String() string {
	return fmt.Sprintf("UUID(%x-%x-%x-%x-%x)", u[0:4], u[4:6], u[6:8], u[8:10], u[10:])
}

// Char is an AMQP unicode character, equivalent to a Go rune.
// It is defined as a distinct type so it can be distinguished from an AMQP int
type Char rune

const intIs64 = unsafe.Sizeof(int(0)) == 8
