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

// #include "codec_shim.h"
import "C"

import (
	"bytes"
	"fmt"
	"io"
	"reflect"
	"unsafe"
)

const minDecode = 1024

// Error returned if AMQP data cannot be unmarshaled as the desired Go type.
type UnmarshalError struct {
	// The name of the AMQP type.
	AMQPType string
	// The Go type.
	GoType reflect.Type
}

func newUnmarshalError(pnType C.pn_type_t, v interface{}) *UnmarshalError {
	return &UnmarshalError{C.pn_type_t(pnType).String(), reflect.TypeOf(v)}
}

func (e UnmarshalError) Error() string {
	if e.GoType.Kind() != reflect.Ptr {
		return fmt.Sprintf("cannot unmarshal to type %s, not a pointer", e.GoType)
	} else {
		return fmt.Sprintf("cannot unmarshal AMQP %s to %s", e.AMQPType, e.GoType)
	}
}

func doRecover(err *error) {
	r := recover()
	switch r := r.(type) {
	case nil:
	case *UnmarshalError:
		*err = r
	default:
		panic(r)
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
	defer doRecover(&err)
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
Unmarshal decodes AMQP-encoded bytes and stores the result in the value pointed to by v.
Types are converted as follows:

 +---------------------------+----------------------------------------------------------------------+
 |To Go types                |From AMQP types                                                       |
 +===========================+======================================================================+
 |bool                       |bool                                                                  |
 +---------------------------+----------------------------------------------------------------------+
 |int, int8, int16,          |Equivalent or smaller signed integer type: byte, short, int, long.    |
 |int32, int64               |                                                                      |
 +---------------------------+----------------------------------------------------------------------+
 |uint, uint8, uint16,       |Equivalent or smaller unsigned integer type: ubyte, ushort, uint,     |
 |uint32, uint64 types       |ulong                                                                 |
 +---------------------------+----------------------------------------------------------------------+
 |float32, float64           |Equivalent or smaller float or double.                                |
 +---------------------------+----------------------------------------------------------------------+
 |string, []byte             |string, symbol or binary.                                             |
 +---------------------------+----------------------------------------------------------------------+
 |Symbol                     |symbol                                                                |
 +---------------------------+----------------------------------------------------------------------+
 |map[K]T                    |map, provided all keys and values can unmarshal to types K, T         |
 +---------------------------+----------------------------------------------------------------------+
 |Map                        |map, any AMQP map                                                     |
 +---------------------------+----------------------------------------------------------------------+
 |interface{}                |Any AMQP value can be unmarshaled to an interface{} as follows:       |
 |                           +------------------------+---------------------------------------------+
 |                           |AMQP Type               |Go Type in interface{}                       |
 |                           +========================+=============================================+
 |                           |bool                    |bool                                         |
 |                           +------------------------+---------------------------------------------+
 |                           |byte,short,int,long     |int8,int16,int32,int64                       |
 |                           +------------------------+---------------------------------------------+
 |                           |ubyte,ushort,uint,ulong |uint8,uint16,uint32,uint64                   |
 |                           +------------------------+---------------------------------------------+
 |                           |float, double           |float32, float64                             |
 |                           +------------------------+---------------------------------------------+
 |                           |string                  |string                                       |
 |                           +------------------------+---------------------------------------------+
 |                           |symbol                  |Symbol                                       |
 |                           +------------------------+---------------------------------------------+
 |                           |binary                  |Binary                                       |
 |                           +------------------------+---------------------------------------------+
 |                           |nulll                   |nil                                          |
 |                           +------------------------+---------------------------------------------+
 |                           |map                     |Map                                          |
 |                           +------------------------+---------------------------------------------+
 |                           |list                    |List                                         |
 +---------------------------+------------------------+---------------------------------------------+

The following Go types cannot be unmarshaled: uintptr, function, interface, channel.

TODO

Go types: array, struct.

AMQP types: decimal32/64/128, char (round trip), timestamp, uuid, array, multi-section message bodies.

AMQP maps with mixed/unhashable key types need an alternate representation.

Described types.
*/
func Unmarshal(bytes []byte, v interface{}) (n int, err error) {
	defer doRecover(&err)

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
			if unsafe.Sizeof(0) == 8 {
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
			if unsafe.Sizeof(0) == 8 {
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

	default:
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
	err := dataError("unmarshaling", data)
	if err != nil {
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
	case C.PN_NULL, C.PN_INVALID: // No data.
		*v = nil
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
	default:
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
	case C.PN_INVALID: // Leave the map empty
	default:
		panic(newUnmarshalError(pnType, v))
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
