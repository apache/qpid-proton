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

package proton

// #include <proton/codec.h>
import "C"

import (
	"bytes"
	"io"
	"reflect"
	"unsafe"
)

const minDecode = 256

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
	defer func() {
		if x := recover(); x != nil {
			err = errorf("%v", x)
		}
	}()
	data := C.pn_data(0)
	defer C.pn_data_free(data)
	var n int
	for n == 0 && err == nil {
		n = unmarshal(data, d.buffer.Bytes(), v)
		if n == 0 { // n == 0 means not enough data, read more
			err = d.more()
			if err != nil {
				return
			}
		}
	}
	d.buffer.Next(n)
	return
}

/*
Unmarshal decodes AMQP-encoded bytes and stores the result in the value pointed to by v.

Go types that can be unmarshalled from AMQP types

bool from AMQP bool

int, int8, int16, int32, int64 from equivalent or smaller AMQP signed integer type or char

uint, uint8, uint16, uint32, uint64 types from equivalent or smaller AMQP unsigned integer type or char

float32, float64 from equivalent or smaller AMQP float type.

string, []byte from AMQP string, symbol or binary.

TODO types

AMQP from AMQP null, char, timestamp, decimal32/64/128, uuid, described, array, list, map

Go: array, slice, struct, map, reflect/Value

Go types that cannot be unmarshalled

complex64/128, uintptr, function, interface, channel
*/
func Unmarshal(bytes []byte, v interface{}) (n int, err error) {
	defer func() {
		if x := recover(); x != nil {
			err = errorf("%v", x)
		}
	}()
	data := C.pn_data(0)
	defer C.pn_data_free(data)
	n = unmarshal(data, bytes, v)
	if n == 0 {
		err = errorf("not enough data")
	}
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

// unmarshal decodes from bytes and converts into the value pointed to by v.
// Used by Unmarshal and Decode
//
// Returns the number of bytes decoded or 0 if not enough data.
//
func unmarshal(data *C.pn_data_t, bytes []byte, v interface{}) (n int) {
	n = decode(data, bytes)
	if n == 0 {
		return 0
	}
	pnType := C.pn_data_type(data)

	badUnmarshal := func() error {
		notPtr := ""
		if reflect.TypeOf(v).Kind() != reflect.Ptr {
			notPtr = ", target is not a pointer"
		}
		return errorf("cannot unmarshal AMQP %s to %s%s", getPnType(pnType), reflect.TypeOf(v), notPtr)
	}

	switch v := v.(type) {
	case *bool:
		switch pnType {
		case C.PN_BOOL:
			*v = bool(C.pn_data_get_bool(data))
		default:
			panic(badUnmarshal())
		}
	case *int8:
		switch pnType {
		case C.PN_CHAR:
			*v = int8(C.pn_data_get_char(data))
		case C.PN_BYTE:
			*v = int8(C.pn_data_get_byte(data))
		default:
			panic(badUnmarshal())
		}
	case *uint8:
		switch pnType {
		case C.PN_CHAR:
			*v = uint8(C.pn_data_get_char(data))
		case C.PN_UBYTE:
			*v = uint8(C.pn_data_get_ubyte(data))
		default:
			panic(badUnmarshal())
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
			panic(badUnmarshal())
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
			panic(badUnmarshal())
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
			panic(badUnmarshal())
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
			panic(badUnmarshal())
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
			panic(badUnmarshal())
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
			panic(badUnmarshal())
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
				panic(badUnmarshal())
			}
		default:
			panic(badUnmarshal())
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
				panic(badUnmarshal())
			}
		default:
			panic(badUnmarshal())
		}

	case *float32:
		switch pnType {
		case C.PN_FLOAT:
			*v = float32(C.pn_data_get_float(data))
		default:
			panic(badUnmarshal())
		}

	case *float64:
		switch pnType {
		case C.PN_FLOAT:
			*v = float64(C.pn_data_get_float(data))
		case C.PN_DOUBLE:
			*v = float64(C.pn_data_get_double(data))
		default:
			panic(badUnmarshal())
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
			panic(badUnmarshal())
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
			panic(badUnmarshal())
		}

	case *reflect.Value:
		switch pnType {
		case C.PN_BOOL:
			*v = reflect.ValueOf(bool(C.pn_data_get_bool(data)))
		case C.PN_UBYTE:
			*v = reflect.ValueOf(uint8(C.pn_data_get_ubyte(data)))
		case C.PN_BYTE:
			*v = reflect.ValueOf(int8(C.pn_data_get_byte(data)))
		case C.PN_USHORT:
			*v = reflect.ValueOf(uint16(C.pn_data_get_ushort(data)))
		case C.PN_SHORT:
			*v = reflect.ValueOf(int16(C.pn_data_get_short(data)))
		case C.PN_UINT:
			*v = reflect.ValueOf(uint32(C.pn_data_get_uint(data)))
		case C.PN_INT:
			*v = reflect.ValueOf(int32(C.pn_data_get_int(data)))
		case C.PN_CHAR:
			*v = reflect.ValueOf(uint8(C.pn_data_get_char(data)))
		case C.PN_ULONG:
			*v = reflect.ValueOf(uint64(C.pn_data_get_ulong(data)))
		case C.PN_LONG:
			*v = reflect.ValueOf(int64(C.pn_data_get_long(data)))
		case C.PN_FLOAT:
			*v = reflect.ValueOf(float32(C.pn_data_get_float(data)))
		case C.PN_DOUBLE:
			*v = reflect.ValueOf(float64(C.pn_data_get_double(data)))
		case C.PN_BINARY:
			*v = valueOfBytes(C.pn_data_get_binary(data))
		case C.PN_STRING:
			*v = valueOfString(C.pn_data_get_string(data))
		case C.PN_SYMBOL:
			*v = valueOfString(C.pn_data_get_symbol(data))
		default:
			panic(badUnmarshal())
		}

	default:
		panic(badUnmarshal())
	}
	err := pnDataError(data)
	if err != "" {
		panic(errorf("unmarshal %s", err))
	}
	return
}

// decode from bytes.
// Return bytes decoded or 0 if we could not decode a complete object.
//
func decode(data *C.pn_data_t, bytes []byte) int {
	if len(bytes) == 0 {
		return 0
	}
	cBuf := (*C.char)(unsafe.Pointer(&bytes[0]))
	n := int(C.pn_data_decode(data, cBuf, C.size_t(len(bytes))))
	if n == int(C.PN_EOS) {
		return 0
	} else if n <= 0 {
		panic(errorf("unmarshal %s", pnErrorName(n)))
	}
	return n
}

func valueOfBytes(bytes C.pn_bytes_t) reflect.Value {
	if bytes.start == nil || bytes.size == 0 {
		return reflect.ValueOf([]byte{})
	}
	return reflect.ValueOf(C.GoBytes(unsafe.Pointer(bytes.start), C.int(bytes.size)))
}

func valueOfString(bytes C.pn_bytes_t) reflect.Value {
	if bytes.start == nil || bytes.size == 0 {
		return reflect.ValueOf("")
	}
	return reflect.ValueOf(C.GoStringN(bytes.start, C.int(bytes.size)))
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
