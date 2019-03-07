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
	"fmt"
	"io"
	"reflect"
	"time"
	"unsafe"
)

// Error returned if Go data cannot be marshaled as an AMQP type.
type MarshalError struct {
	// The Go type.
	GoType reflect.Type
	s      string
}

func (e MarshalError) Error() string { return e.s }

func newMarshalError(v interface{}, s string) *MarshalError {
	t := reflect.TypeOf(v)
	return &MarshalError{GoType: t, s: fmt.Sprintf("cannot marshal %s: %s", t, s)}
}

func dataMarshalError(v interface{}, data *C.pn_data_t) error {
	if pe := PnError(C.pn_data_error(data)); pe != nil {
		return newMarshalError(v, pe.Error())
	}
	return nil
}

/*
Marshal encodes a Go value as AMQP data in buffer.
If buffer is nil, or is not large enough, a new buffer  is created.

Returns the buffer used for encoding with len() adjusted to the actual size of data.

Go types are encoded as follows

 +-------------------------------------+--------------------------------------------+
 |Go type                              |AMQP type                                   |
 +-------------------------------------+--------------------------------------------+
 |bool                                 |bool                                        |
 +-------------------------------------+--------------------------------------------+
 |int8, int16, int32, int64 (int)      |byte, short, int, long (int or long)        |
 +-------------------------------------+--------------------------------------------+
 |uint8, uint16, uint32, uint64 (uint) |ubyte, ushort, uint, ulong (uint or ulong)  |
 +-------------------------------------+--------------------------------------------+
 |float32, float64                     |float, double.                              |
 +-------------------------------------+--------------------------------------------+
 |string                               |string                                      |
 +-------------------------------------+--------------------------------------------+
 |[]byte, Binary                       |binary                                      |
 +-------------------------------------+--------------------------------------------+
 |Symbol                               |symbol                                      |
 +-------------------------------------+--------------------------------------------+
 |Char                                 |char                                        |
 +-------------------------------------+--------------------------------------------+
 |interface{}                          |the contained type                          |
 +-------------------------------------+--------------------------------------------+
 |nil                                  |null                                        |
 +-------------------------------------+--------------------------------------------+
 |map[K]T                              |map with K and T converted as above         |
 +-------------------------------------+--------------------------------------------+
 |Map                                  |map, may have mixed types for keys, values  |
 +-------------------------------------+--------------------------------------------+
 |AnyMap                               |map (See AnyMap)                            |
 +-------------------------------------+--------------------------------------------+
 |List, []interface{}                  |list, may have mixed-type values            |
 +-------------------------------------+--------------------------------------------+
 |[]T, [N]T                            |array, T is mapped as per this table        |
 +-------------------------------------+--------------------------------------------+
 |Described                            |described type                              |
 +-------------------------------------+--------------------------------------------+
 |time.Time                            |timestamp                                   |
 +-------------------------------------+--------------------------------------------+
 |UUID                                 |uuid                                        |
 +-------------------------------------+--------------------------------------------+

The following Go types cannot be marshaled: uintptr, function, channel, struct, complex64/128

AMQP types not yet supported: decimal32/64/128
*/

func Marshal(v interface{}, buffer []byte) (outbuf []byte, err error) {
	data := C.pn_data(0)
	defer C.pn_data_free(data)
	if err = recoverMarshal(v, data); err != nil {
		return buffer, err
	}
	encode := func(buf []byte) ([]byte, error) {
		n := int(C.pn_data_encode(data, cPtr(buf), cLen(buf)))
		switch {
		case n == int(C.PN_OVERFLOW):
			return buf, overflow
		case n < 0:
			return buf, dataMarshalError(v, data)
		default:
			return buf[:n], nil
		}
	}
	return encodeGrow(buffer, encode)
}

// Internal use only
func MarshalUnsafe(v interface{}, pnData unsafe.Pointer) (err error) {
	return recoverMarshal(v, (*C.pn_data_t)(pnData))
}

func recoverMarshal(v interface{}, data *C.pn_data_t) (err error) {
	defer func() { // Convert panic to error return
		if r := recover(); r != nil {
			if err2, ok := r.(*MarshalError); ok {
				err = err2 // Convert internal panic to error
			} else {
				panic(r) // Unrecognized error, continue to panic
			}
		}
	}()
	marshal(v, data) // Panics on error
	return
}

const minEncode = 256

// overflow is returned when an encoding function can't fit data in the buffer.
var overflow = fmt.Errorf("buffer too small")

// encodeFn encodes into buffer[0:len(buffer)].
// Returns buffer with length adjusted for data encoded.
// If buffer too small, returns overflow as error.
type encodeFn func(buffer []byte) ([]byte, error)

// encodeGrow calls encode() into buffer, if it returns overflow grows the buffer.
// Returns the final buffer.
func encodeGrow(buffer []byte, encode encodeFn) ([]byte, error) {
	if buffer == nil || len(buffer) == 0 {
		buffer = make([]byte, minEncode)
	}
	var err error
	for buffer, err = encode(buffer); err == overflow; buffer, err = encode(buffer) {
		buffer = make([]byte, 2*len(buffer))
	}
	return buffer, err
}

// Marshal v to data
func marshal(i interface{}, data *C.pn_data_t) {
	switch v := i.(type) {
	case nil:
		C.pn_data_put_null(data)
	case bool:
		C.pn_data_put_bool(data, C.bool(v))

	// Signed integers
	case int8:
		C.pn_data_put_byte(data, C.int8_t(v))
	case int16:
		C.pn_data_put_short(data, C.int16_t(v))
	case int32:
		C.pn_data_put_int(data, C.int32_t(v))
	case int64:
		C.pn_data_put_long(data, C.int64_t(v))
	case int:
		if intIs64 {
			C.pn_data_put_long(data, C.int64_t(v))
		} else {
			C.pn_data_put_int(data, C.int32_t(v))
		}

		// Unsigned integers
	case uint8:
		C.pn_data_put_ubyte(data, C.uint8_t(v))
	case uint16:
		C.pn_data_put_ushort(data, C.uint16_t(v))
	case uint32:
		C.pn_data_put_uint(data, C.uint32_t(v))
	case uint64:
		C.pn_data_put_ulong(data, C.uint64_t(v))
	case uint:
		if intIs64 {
			C.pn_data_put_ulong(data, C.uint64_t(v))
		} else {
			C.pn_data_put_uint(data, C.uint32_t(v))
		}

		// Floating point
	case float32:
		C.pn_data_put_float(data, C.float(v))
	case float64:
		C.pn_data_put_double(data, C.double(v))

		// String-like (string, binary, symbol)
	case string:
		C.pn_data_put_string(data, pnBytes([]byte(v)))
	case []byte:
		C.pn_data_put_binary(data, pnBytes(v))
	case Binary:
		C.pn_data_put_binary(data, pnBytes([]byte(v)))
	case Symbol:
		C.pn_data_put_symbol(data, pnBytes([]byte(v)))

		// Other simple types
	case time.Time:
		C.pn_data_put_timestamp(data, pnTime(v))
	case UUID:
		C.pn_data_put_uuid(data, *(*C.pn_uuid_t)(unsafe.Pointer(&v[0])))
	case Char:
		C.pn_data_put_char(data, (C.pn_char_t)(v))

		// Described types
	case Described:
		C.pn_data_put_described(data)
		C.pn_data_enter(data)
		marshal(v.Descriptor, data)
		marshal(v.Value, data)
		C.pn_data_exit(data)

		// Restricted type annotation-key, marshals as contained value
	case AnnotationKey:
		marshal(v.Get(), data)

		// Special type to represent AMQP maps with keys that are illegal in Go
	case AnyMap:
		C.pn_data_put_map(data)
		C.pn_data_enter(data)
		defer C.pn_data_exit(data)
		for _, kv := range v {
			marshal(kv.Key, data)
			marshal(kv.Value, data)
		}

	default:
		// Examine complex types (Go map, slice, array) by reflected structure
		switch reflect.TypeOf(i).Kind() {

		case reflect.Map:
			m := reflect.ValueOf(v)
			C.pn_data_put_map(data)
			if C.pn_data_enter(data) {
				defer C.pn_data_exit(data)
			} else {
				panic(dataMarshalError(i, data))
			}
			for _, key := range m.MapKeys() {
				marshal(key.Interface(), data)
				marshal(m.MapIndex(key).Interface(), data)
			}

		case reflect.Slice, reflect.Array:
			// Note: Go array and slice are mapped the same way:
			// if element type is an interface, map to AMQP list (mixed type)
			// if element type is a non-interface type map to AMQP array (single type)
			s := reflect.ValueOf(v)
			if pnType, ok := arrayTypeMap[s.Type().Elem()]; ok {
				C.pn_data_put_array(data, false, pnType)
			} else {
				C.pn_data_put_list(data)
			}
			C.pn_data_enter(data)
			defer C.pn_data_exit(data)
			for j := 0; j < s.Len(); j++ {
				marshal(s.Index(j).Interface(), data)
			}

		default:
			panic(newMarshalError(v, "no conversion"))
		}
	}
	if err := dataMarshalError(i, data); err != nil {
		panic(err)
	}
}

// Mapping froo Go element type to AMQP array type for types that can go in an AMQP array
// NOTE: this must be kept consistent with marshal() which does the actual marshalling.
var arrayTypeMap = map[reflect.Type]C.pn_type_t{
	nil:                  C.PN_NULL,
	reflect.TypeOf(true): C.PN_BOOL,

	reflect.TypeOf(int8(0)):  C.PN_BYTE,
	reflect.TypeOf(int16(0)): C.PN_INT,
	reflect.TypeOf(int32(0)): C.PN_SHORT,
	reflect.TypeOf(int64(0)): C.PN_LONG,

	reflect.TypeOf(uint8(0)):  C.PN_UBYTE,
	reflect.TypeOf(uint16(0)): C.PN_UINT,
	reflect.TypeOf(uint32(0)): C.PN_USHORT,
	reflect.TypeOf(uint64(0)): C.PN_ULONG,

	reflect.TypeOf(float32(0)): C.PN_FLOAT,
	reflect.TypeOf(float64(0)): C.PN_DOUBLE,

	reflect.TypeOf(""):                    C.PN_STRING,
	reflect.TypeOf((*Symbol)(nil)).Elem(): C.PN_SYMBOL,
	reflect.TypeOf((*Binary)(nil)).Elem(): C.PN_BINARY,
	reflect.TypeOf([]byte{}):              C.PN_BINARY,

	reflect.TypeOf((*time.Time)(nil)).Elem(): C.PN_TIMESTAMP,
	reflect.TypeOf((*UUID)(nil)).Elem():      C.PN_UUID,
	reflect.TypeOf((*Char)(nil)).Elem():      C.PN_CHAR,
}

// Compute mapping of int/uint at runtime as they depend on execution environment.
func init() {
	if intIs64 {
		arrayTypeMap[reflect.TypeOf(int(0))] = C.PN_LONG
		arrayTypeMap[reflect.TypeOf(uint(0))] = C.PN_ULONG
	} else {
		arrayTypeMap[reflect.TypeOf(int(0))] = C.PN_INT
		arrayTypeMap[reflect.TypeOf(uint(0))] = C.PN_UINT
	}
}

func clearMarshal(v interface{}, data *C.pn_data_t) {
	C.pn_data_clear(data)
	marshal(v, data)
}

// Encoder encodes AMQP values to an io.Writer
type Encoder struct {
	writer io.Writer
	buffer []byte
}

// New encoder returns a new encoder that writes to w.
func NewEncoder(w io.Writer) *Encoder {
	return &Encoder{w, make([]byte, minEncode)}
}

func (e *Encoder) Encode(v interface{}) (err error) {
	e.buffer, err = Marshal(v, e.buffer)
	if err == nil {
		_, err = e.writer.Write(e.buffer)
	}
	return err
}
