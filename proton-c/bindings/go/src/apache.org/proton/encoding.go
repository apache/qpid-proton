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
	for n == 0 {
		n = unmarshal(data, d.buffer.Bytes(), v)
		if n == 0 { // n == 0 means not enough data, read more
			err = d.more()
		}
	}
	d.buffer.Next(n)
	return
}

/*
Unmarshal decodes AMQP-encoded bytes and stores the result in the value pointed to by v.

Go types that can be unmarshalled from AMQP types

bool from AMQP bool

int8, int16, int32, int64 from equivalent or smaller AMQP signed integer type.

uint8, uint16, uint32, uint64 types from equivalent or smaller AMQP unsigned integer type.

float32, float64 from equivalent or smaller AMQP float type.

string, []byte from AMQP string, symbol or binary.

TODO types

AMQP from AMQP null, char, timestamp, decimal32/64/128, uuid, described, array, list, map

Go: uint, int, array, slice, struct, map, reflect/Value

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

func pnDataError(data *C.pn_data_t) (code int, msg string) {
	pnError := C.pn_data_error(data)
	return int(C.pn_error_code(pnError)), C.GoString(C.pn_error_text(pnError))
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

// more reads more data when we can't parse a complete AMQP type
func (d *Decoder) more() error {
	var readSize int64 = 256
	if int64(d.buffer.Len()) > readSize { // Grow by doubling
		readSize = int64(d.buffer.Len())
	}
	var n int64
	n, err := d.buffer.ReadFrom(&io.LimitedReader{d.reader, readSize})
	if n == 0 { // ReadFrom won't report io.EOF, just returns 0
		if err != nil {
			panic(err)
		} else {
			panic("not enough data")
		}
	}
	return nil
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
	ptrValue := reflect.ValueOf(v)
	if ptrValue.Type().Kind() != reflect.Ptr {
		panic(errorf("cannot unmarshal to %T, not a pointer", v))
	}
	value := ptrValue.Elem()
	pnType, ok := pnTypes[C.pn_data_type(data)]
	if !ok {
		panic(errorf("unknown AMQP type code %v", C.pn_data_type(data)))
	}
	if pnType.getter == nil {
		panic(errorf("cannot unmarshal AMQP type %s", pnType.name))
	}
	decoded := pnType.getter(data)
	if !decoded.Type().ConvertibleTo(value.Type()) {
		panic(errorf("cannot unmarshal AMQP %s to %s", pnType.name, value.Type()))
	}
	converted := decoded.Convert(value.Type())
	value.Set(converted)
	return
}

func bytesValue(bytes C.pn_bytes_t) reflect.Value {
	if bytes.start == nil || bytes.size == 0 {
		return reflect.ValueOf([]byte{})
	}
	return reflect.ValueOf(C.GoBytes(unsafe.Pointer(bytes.start), C.int(bytes.size)))
}

// Get functions to convert PN data to reflect.Value
func getPnString(data *C.pn_data_t) reflect.Value { return bytesValue(C.pn_data_get_string(data)) }
func getPnSymbol(data *C.pn_data_t) reflect.Value { return bytesValue(C.pn_data_get_symbol(data)) }
func getPnBinary(data *C.pn_data_t) reflect.Value { return bytesValue(C.pn_data_get_binary(data)) }
func getPnBool(data *C.pn_data_t) reflect.Value   { return reflect.ValueOf(C.pn_data_get_bool(data)) }
func getPnByte(data *C.pn_data_t) reflect.Value   { return reflect.ValueOf(C.pn_data_get_byte(data)) }
func getPnChar(data *C.pn_data_t) reflect.Value   { return reflect.ValueOf(C.pn_data_get_char(data)) }
func getPnShort(data *C.pn_data_t) reflect.Value  { return reflect.ValueOf(C.pn_data_get_short(data)) }
func getPnInt(data *C.pn_data_t) reflect.Value    { return reflect.ValueOf(C.pn_data_get_int(data)) }
func getPnLong(data *C.pn_data_t) reflect.Value   { return reflect.ValueOf(C.pn_data_get_long(data)) }
func getPnUbyte(data *C.pn_data_t) reflect.Value  { return reflect.ValueOf(C.pn_data_get_ubyte(data)) }
func getPnUshort(data *C.pn_data_t) reflect.Value { return reflect.ValueOf(C.pn_data_get_ushort(data)) }
func getPnUint(data *C.pn_data_t) reflect.Value   { return reflect.ValueOf(C.pn_data_get_uint(data)) }
func getPnUlong(data *C.pn_data_t) reflect.Value  { return reflect.ValueOf(C.pn_data_get_ulong(data)) }
func getPnFloat(data *C.pn_data_t) reflect.Value  { return reflect.ValueOf(C.pn_data_get_float(data)) }
func getPnDouble(data *C.pn_data_t) reflect.Value { return reflect.ValueOf(C.pn_data_get_double(data)) }
