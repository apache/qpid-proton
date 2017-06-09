/*
Licensed to the Apache Software Foundation (ASF) under one
oor more contributor license agreements.  See the NOTICE file
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
	"io"
	"reflect"
	"strings"
	"unsafe"
)

const minDecode = 1024

// Error returned if AMQP data cannot be unmarshaled as the desired Go type.
type UnmarshalError struct {
	// The name of the AMQP type.
	AMQPType string
	// The Go type.
	GoType reflect.Type

	s string
}

func (e UnmarshalError) Error() string { return e.s }

func newUnmarshalErrorMsg(pnType C.pn_type_t, v interface{}, msg string) *UnmarshalError {
	if len(msg) > 0 && !strings.HasPrefix(msg, ":") {
		msg = ": " + msg
	}
	e := &UnmarshalError{AMQPType: C.pn_type_t(pnType).String(), GoType: reflect.TypeOf(v)}
	if e.GoType.Kind() != reflect.Ptr {
		e.s = fmt.Sprintf("cannot unmarshal to type %s, not a pointer%s", e.GoType, msg)
	} else {
		e.s = fmt.Sprintf("cannot unmarshal AMQP %s to %s%s", e.AMQPType, e.GoType, msg)
	}
	return e
}

func newUnmarshalError(pnType C.pn_type_t, v interface{}) *UnmarshalError {
	return newUnmarshalErrorMsg(pnType, v, "")
}

func newUnmarshalErrorData(data *C.pn_data_t, v interface{}) *UnmarshalError {
	err := PnError(C.pn_data_error(data))
	if err == nil {
		return nil
	}
	e := newUnmarshalError(C.pn_data_type(data), v)
	e.s = e.s + ": " + err.Error()
	return e
}

func recoverUnmarshal(err *error) {
	if r := recover(); r != nil {
		if uerr, ok := r.(*UnmarshalError); ok {
			*err = uerr
		} else {
			panic(r)
		}
	}
}

//
// Decoding from a pn_data_t
//
// NOTE: we use panic() to signal a decoding error, simplifies decoding logic.
// We recover() at the highest possible level - i.e. in the exported Unmarshal or Decode.
//

// Decoder decodes AMQP values from an io.Reader.
//
type Decoder struct {
	reader io.Reader
	buffer bytes.Buffer
}

// NewDecoder returns a new decoder that reads from r.
//
// The decoder has it's own buffer and may read more data than required for the
// AMQP values requested.  Use Buffered to see if there is data left in the
// buffer.
//
func NewDecoder(r io.Reader) *Decoder {
	return &Decoder{r, bytes.Buffer{}}
}

// Buffered returns a reader of the data remaining in the Decoder's buffer. The
// reader is valid until the next call to Decode.
//
func (d *Decoder) Buffered() io.Reader {
	return bytes.NewReader(d.buffer.Bytes())
}

// Decode reads the next AMQP value from the Reader and stores it in the value pointed to by v.
//
// See the documentation for Unmarshal for details about the conversion of AMQP into a Go value.
//
func (d *Decoder) Decode(v interface{}) (err error) {
	defer recoverUnmarshal(&err)
	data := C.pn_data(0)
	defer C.pn_data_free(data)
	var n int
	for n == 0 {
		n, err = decode(data, d.buffer.Bytes())
		if err != nil {
			return err
		}
		if n == 0 { // n == 0 means not enough data, read more
			err = d.more()
		} else {
			unmarshal(v, data)
		}
	}
	d.buffer.Next(n)
	return
}

/*
Unmarshal decodes AMQP-encoded bytes and stores the result in the Go value pointed to by v.
Types are converted as follows:

 +------------------------+-------------------------------------------------+
 |To Go types             |From AMQP types                                  |
 +========================+=================================================+
 |bool                    |bool                                             |
 +------------------------+-------------------------------------------------+
 |int, int8, int16, int32,|Equivalent or smaller signed integer type: byte, |
 |int64                   |short, int, long.                                |
 +------------------------+-------------------------------------------------+
 |uint, uint8, uint16,    |Equivalent or smaller unsigned integer type:     |
 |uint32, uint64          |ubyte, ushort, uint, ulong                       |
 +------------------------+-------------------------------------------------+
 |float32, float64        |Equivalent or smaller float or double.           |
 +------------------------+-------------------------------------------------+
 |string, []byte          |string, symbol or binary.                        |
 +------------------------+-------------------------------------------------+
 |Symbol                  |symbol                                           |
 +------------------------+-------------------------------------------------+
 |map[K]T                 |map, provided all keys and values can unmarshal  |
 |                        |to types K,T                                     |
 +------------------------+-------------------------------------------------+
 |Map                     |map, any AMQP map                                |
 +------------------------+-------------------------------------------------+
 |Described               |described type                                   |
 +------------------------+-------------------------------------------------+

An AMQP described type can unmarshal into the corresponding plain type, discarding the descriptor.
For example an AMQP described string can unmarshal into a plain go string.
Unmarshal into the Described type preserves the descriptor.

Any AMQP type can unmarshal to an interface{}, the Go type used to unmarshal is chosen from the AMQP type as follows

 +------------------------+-------------------------------------------------+
 |AMQP Type               |Go Type in interface{}                           |
 +========================+=================================================+
 |bool                    |bool                                             |
 +------------------------+-------------------------------------------------+
 |byte,short,int,long     |int8,int16,int32,int64                           |
 +------------------------+-------------------------------------------------+
 |ubyte,ushort,uint,ulong |uint8,uint16,uint32,uint64                       |
 +------------------------+-------------------------------------------------+
 |float, double           |float32, float64                                 |
 +------------------------+-------------------------------------------------+
 |string                  |string                                           |
 +------------------------+-------------------------------------------------+
 |symbol                  |Symbol                                           |
 +------------------------+-------------------------------------------------+
 |binary                  |Binary                                           |
 +------------------------+-------------------------------------------------+
 |null                    |nil                                              |
 +------------------------+-------------------------------------------------+
 |map                     |Map                                              |
 +------------------------+-------------------------------------------------+
 |list                    |List                                             |
 +------------------------+-------------------------------------------------+
 |described type          |Described                                        |
 +--------------------------------------------------------------------------+

The following Go types cannot be unmarshaled: uintptr, function, interface, channel, array (use slice), struct

TODO: Not yet implemented:

AMQP types: decimal32/64/128, char (round trip), timestamp, uuid.

AMQP maps with mixed key types, or key types that are not legal Go map keys.
*/
func Unmarshal(bytes []byte, v interface{}) (n int, err error) {
	defer recoverUnmarshal(&err)

	data := C.pn_data(0)
	defer C.pn_data_free(data)
	n, err = decode(data, bytes)
	if err != nil {
		return 0, err
	}
	if n == 0 {
		return 0, fmt.Errorf("not enough data")
	} else {
		unmarshal(v, data)
	}
	return n, nil
}

// Internal
func UnmarshalUnsafe(pn_data unsafe.Pointer, v interface{}) (err error) {
	defer recoverUnmarshal(&err)
	unmarshal(v, (*C.pn_data_t)(pn_data))
	return
}

// more reads more data when we can't parse a complete AMQP type
func (d *Decoder) more() error {
	var readSize int64 = minDecode
	if int64(d.buffer.Len()) > readSize { // Grow by doubling
		readSize = int64(d.buffer.Len())
	}
	var n int64
	n, err := d.buffer.ReadFrom(io.LimitReader(d.reader, readSize))
	if n == 0 && err == nil { // ReadFrom won't report io.EOF, just returns 0
		err = io.EOF
	}
	return err
}

// Unmarshal from data into value pointed at by v.
func unmarshal(v interface{}, data *C.pn_data_t) {
	pnType := C.pn_data_type(data)

	// Check for PN_DESCRIBED first, as described types can unmarshal into any of the Go types.
	// Interfaces are handled in the switch below, even for described types.
	if _, isInterface := v.(*interface{}); !isInterface && bool(C.pn_data_is_described(data)) {
		getDescribed(data, v)
		return
	}

	// Unmarshal based on the target type
	switch v := v.(type) {
	case *bool:
		switch pnType {
		case C.PN_BOOL:
			*v = bool(C.pn_data_get_bool(data))
		default:
			panic(newUnmarshalError(pnType, v))
		}
	case *int8:
		switch pnType {
		case C.PN_CHAR:
			*v = int8(C.pn_data_get_char(data))
		case C.PN_BYTE:
			*v = int8(C.pn_data_get_byte(data))
		default:
			panic(newUnmarshalError(pnType, v))
		}
	case *uint8:
		switch pnType {
		case C.PN_CHAR:
			*v = uint8(C.pn_data_get_char(data))
		case C.PN_UBYTE:
			*v = uint8(C.pn_data_get_ubyte(data))
		default:
			panic(newUnmarshalError(pnType, v))
		}
	case *int16:
		switch pnType {
		case C.PN_CHAR:
			*v = int16(C.pn_data_get_char(data))
		case C.PN_BYTE:
			*v = int16(C.pn_data_get_byte(data))
		case C.PN_SHORT:
			*v = int16(C.pn_data_get_short(data))
		default:
			panic(newUnmarshalError(pnType, v))
		}
	case *uint16:
		switch pnType {
		case C.PN_CHAR:
			*v = uint16(C.pn_data_get_char(data))
		case C.PN_UBYTE:
			*v = uint16(C.pn_data_get_ubyte(data))
		case C.PN_USHORT:
			*v = uint16(C.pn_data_get_ushort(data))
		default:
			panic(newUnmarshalError(pnType, v))
		}
	case *int32:
		switch pnType {
		case C.PN_CHAR:
			*v = int32(C.pn_data_get_char(data))
		case C.PN_BYTE:
			*v = int32(C.pn_data_get_byte(data))
		case C.PN_SHORT:
			*v = int32(C.pn_data_get_short(data))
		case C.PN_INT:
			*v = int32(C.pn_data_get_int(data))
		default:
			panic(newUnmarshalError(pnType, v))
		}
	case *uint32:
		switch pnType {
		case C.PN_CHAR:
			*v = uint32(C.pn_data_get_char(data))
		case C.PN_UBYTE:
			*v = uint32(C.pn_data_get_ubyte(data))
		case C.PN_USHORT:
			*v = uint32(C.pn_data_get_ushort(data))
		case C.PN_UINT:
			*v = uint32(C.pn_data_get_uint(data))
		default:
			panic(newUnmarshalError(pnType, v))
		}

	case *int64:
		switch pnType {
		case C.PN_CHAR:
			*v = int64(C.pn_data_get_char(data))
		case C.PN_BYTE:
			*v = int64(C.pn_data_get_byte(data))
		case C.PN_SHORT:
			*v = int64(C.pn_data_get_short(data))
		case C.PN_INT:
			*v = int64(C.pn_data_get_int(data))
		case C.PN_LONG:
			*v = int64(C.pn_data_get_long(data))
		default:
			panic(newUnmarshalError(pnType, v))
		}

	case *uint64:
		switch pnType {
		case C.PN_CHAR:
			*v = uint64(C.pn_data_get_char(data))
		case C.PN_UBYTE:
			*v = uint64(C.pn_data_get_ubyte(data))
		case C.PN_USHORT:
			*v = uint64(C.pn_data_get_ushort(data))
		case C.PN_ULONG:
			*v = uint64(C.pn_data_get_ulong(data))
		default:
			panic(newUnmarshalError(pnType, v))
		}

	case *int:
		switch pnType {
		case C.PN_CHAR:
			*v = int(C.pn_data_get_char(data))
		case C.PN_BYTE:
			*v = int(C.pn_data_get_byte(data))
		case C.PN_SHORT:
			*v = int(C.pn_data_get_short(data))
		case C.PN_INT:
			*v = int(C.pn_data_get_int(data))
		case C.PN_LONG:
			if unsafe.Sizeof(int(0)) == 8 {
				*v = int(C.pn_data_get_long(data))
			} else {
				panic(newUnmarshalError(pnType, v))
			}
		default:
			panic(newUnmarshalError(pnType, v))
		}

	case *uint:
		switch pnType {
		case C.PN_CHAR:
			*v = uint(C.pn_data_get_char(data))
		case C.PN_UBYTE:
			*v = uint(C.pn_data_get_ubyte(data))
		case C.PN_USHORT:
			*v = uint(C.pn_data_get_ushort(data))
		case C.PN_UINT:
			*v = uint(C.pn_data_get_uint(data))
		case C.PN_ULONG:
			if unsafe.Sizeof(int(0)) == 8 {
				*v = uint(C.pn_data_get_ulong(data))
			} else {
				panic(newUnmarshalError(pnType, v))
			}
		default:
			panic(newUnmarshalError(pnType, v))
		}

	case *float32:
		switch pnType {
		case C.PN_FLOAT:
			*v = float32(C.pn_data_get_float(data))
		default:
			panic(newUnmarshalError(pnType, v))
		}

	case *float64:
		switch pnType {
		case C.PN_FLOAT:
			*v = float64(C.pn_data_get_float(data))
		case C.PN_DOUBLE:
			*v = float64(C.pn_data_get_double(data))
		default:
			panic(newUnmarshalError(pnType, v))
		}

	case *string:
		switch pnType {
		case C.PN_STRING:
			*v = goString(C.pn_data_get_string(data))
		case C.PN_SYMBOL:
			*v = goString(C.pn_data_get_symbol(data))
		case C.PN_BINARY:
			*v = goString(C.pn_data_get_binary(data))
		default:
			panic(newUnmarshalError(pnType, v))
		}

	case *[]byte:
		switch pnType {
		case C.PN_STRING:
			*v = goBytes(C.pn_data_get_string(data))
		case C.PN_SYMBOL:
			*v = goBytes(C.pn_data_get_symbol(data))
		case C.PN_BINARY:
			*v = goBytes(C.pn_data_get_binary(data))
		default:
			panic(newUnmarshalError(pnType, v))
		}

	case *Binary:
		switch pnType {
		case C.PN_BINARY:
			*v = Binary(goBytes(C.pn_data_get_binary(data)))
		default:
			panic(newUnmarshalError(pnType, v))
		}

	case *Symbol:
		switch pnType {
		case C.PN_SYMBOL:
			*v = Symbol(goBytes(C.pn_data_get_symbol(data)))
		default:
			panic(newUnmarshalError(pnType, v))
		}

	case *interface{}:
		getInterface(data, v)

	case *AnnotationKey:
		if pnType == C.PN_ULONG || pnType == C.PN_SYMBOL || pnType == C.PN_STRING {
			unmarshal(&v.value, data)
		} else {
			panic(newUnmarshalError(pnType, v))
		}

	default: // This is not one of the fixed well-known types, reflect for map and slice types
		if reflect.TypeOf(v).Kind() != reflect.Ptr {
			panic(newUnmarshalError(pnType, v))
		}
		switch reflect.TypeOf(v).Elem().Kind() {
		case reflect.Map:
			getMap(data, v)
		case reflect.Slice:
			getList(data, v)
		default:
			panic(newUnmarshalError(pnType, v))
		}
	}
	if err := newUnmarshalErrorData(data, v); err != nil {
		panic(err)
	}
	return
}

func rewindUnmarshal(v interface{}, data *C.pn_data_t) {
	C.pn_data_rewind(data)
	C.pn_data_next(data)
	unmarshal(v, data)
}

// Getting into an interface is driven completely by the AMQP type, since the interface{}
// target is type-neutral.
func getInterface(data *C.pn_data_t, v *interface{}) {
	pnType := C.pn_data_type(data)
	switch pnType {
	case C.PN_BOOL:
		*v = bool(C.pn_data_get_bool(data))
	case C.PN_UBYTE:
		*v = uint8(C.pn_data_get_ubyte(data))
	case C.PN_BYTE:
		*v = int8(C.pn_data_get_byte(data))
	case C.PN_USHORT:
		*v = uint16(C.pn_data_get_ushort(data))
	case C.PN_SHORT:
		*v = int16(C.pn_data_get_short(data))
	case C.PN_UINT:
		*v = uint32(C.pn_data_get_uint(data))
	case C.PN_INT:
		*v = int32(C.pn_data_get_int(data))
	case C.PN_CHAR:
		*v = uint8(C.pn_data_get_char(data))
	case C.PN_ULONG:
		*v = uint64(C.pn_data_get_ulong(data))
	case C.PN_LONG:
		*v = int64(C.pn_data_get_long(data))
	case C.PN_FLOAT:
		*v = float32(C.pn_data_get_float(data))
	case C.PN_DOUBLE:
		*v = float64(C.pn_data_get_double(data))
	case C.PN_BINARY:
		*v = Binary(goBytes(C.pn_data_get_binary(data)))
	case C.PN_STRING:
		*v = goString(C.pn_data_get_string(data))
	case C.PN_SYMBOL:
		*v = Symbol(goString(C.pn_data_get_symbol(data)))
	case C.PN_MAP:
		m := make(Map)
		unmarshal(&m, data)
		*v = m
	case C.PN_LIST:
		l := make(List, 0)
		unmarshal(&l, data)
		*v = l
	case C.PN_DESCRIBED:
		d := Described{}
		unmarshal(&d, data)
		*v = d
	case C.PN_NULL:
		*v = nil
	case C.PN_INVALID:
		// Allow decoding from an empty data object to an interface, treat it like NULL.
		// This happens when optional values or properties are omitted from a message.
		*v = nil
	default: // Don't know how to handle this
		panic(newUnmarshalError(pnType, v))
	}
}

// get into map pointed at by v
func getMap(data *C.pn_data_t, v interface{}) {
	mapValue := reflect.ValueOf(v).Elem()
	mapValue.Set(reflect.MakeMap(mapValue.Type())) // Clear the map
	switch pnType := C.pn_data_type(data); pnType {
	case C.PN_MAP:
		count := int(C.pn_data_get_map(data))
		if bool(C.pn_data_enter(data)) {
			defer C.pn_data_exit(data)
			for i := 0; i < count/2; i++ {
				if bool(C.pn_data_next(data)) {
					key := reflect.New(mapValue.Type().Key())
					unmarshal(key.Interface(), data)
					if bool(C.pn_data_next(data)) {
						val := reflect.New(mapValue.Type().Elem())
						unmarshal(val.Interface(), data)
						mapValue.SetMapIndex(key.Elem(), val.Elem())
					}
				}
			}
		}
	default: // Empty/error/unknown, leave map empty
	}
}

func getList(data *C.pn_data_t, v interface{}) {
	pnType := C.pn_data_type(data)
	if pnType != C.PN_LIST {
		panic(newUnmarshalError(pnType, v))
	}
	count := int(C.pn_data_get_list(data))
	listValue := reflect.MakeSlice(reflect.TypeOf(v).Elem(), count, count)
	if bool(C.pn_data_enter(data)) {
		for i := 0; i < count; i++ {
			if bool(C.pn_data_next(data)) {
				val := reflect.New(listValue.Type().Elem())
				unmarshal(val.Interface(), data)
				listValue.Index(i).Set(val.Elem())
			}
		}
		C.pn_data_exit(data)
	}
	reflect.ValueOf(v).Elem().Set(listValue)
}

func getDescribed(data *C.pn_data_t, v interface{}) {
	d, _ := v.(*Described)
	pnType := C.pn_data_type(data)
	if bool(C.pn_data_enter(data)) {
		defer C.pn_data_exit(data)
		if bool(C.pn_data_next(data)) {
			if d != nil {
				unmarshal(&d.Descriptor, data)
			}
			if bool(C.pn_data_next(data)) {
				if d != nil {
					unmarshal(&d.Value, data)
				} else {
					unmarshal(v, data)
				}
				return
			}
		}
	}
	// The pn_data cursor didn't move as expected
	panic(newUnmarshalErrorMsg(pnType, v, "bad described value encoding"))
}

// decode from bytes.
// Return bytes decoded or 0 if we could not decode a complete object.
//
func decode(data *C.pn_data_t, bytes []byte) (int, error) {
	if len(bytes) == 0 {
		return 0, nil
	}
	n := int(C.pn_data_decode(data, cPtr(bytes), cLen(bytes)))
	if n == int(C.PN_UNDERFLOW) {
		C.pn_error_clear(C.pn_data_error(data))
		return 0, nil
	} else if n <= 0 {
		return 0, fmt.Errorf("unmarshal %s", PnErrorCode(n))
	}
	return n, nil
}
