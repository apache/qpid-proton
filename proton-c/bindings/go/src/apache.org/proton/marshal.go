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
	"io"
	"reflect"
	"unsafe"
)

const minEncode = 256

/*
Marshal encodes a value as AMQP.

Go types are encoded as follows

bool to AMQP bool.

int, int8, int16, int32, int64 to equivalent AMQP signed integer type.

uint, uint8, uint16, uint32, uint64 to equivalent or smaller AMQP unsigned integer type.

float32, float64 to AMQP float or double.

string to AMQP string.

[]byte to AMQP binary.

TODO types

Go: array, slice, struct, map, reflect/Value

Go types that cannot be marshaled

complex64/128, uintptr, function, interface, channel

*/
func Marshal(v interface{}) (bytes []byte, err error) {
	defer func() {
		if x := recover(); x != nil {
			err = errorf("%v", x)
		}
	}()
	return marshal(make([]byte, minEncode), v)
}

func marshal(bytesIn []byte, v interface{}) (bytes []byte, err error) {
	data := C.pn_data(0)
	defer C.pn_data_free(data)
	switch v := v.(type) {
	case bool:
		C.pn_data_put_bool(data, C.bool(v))
	case int8:
		C.pn_data_put_byte(data, C.int8_t(v))
	case int16:
		C.pn_data_put_short(data, C.int16_t(v))
	case int32:
		C.pn_data_put_int(data, C.int32_t(v))
	case int64:
		C.pn_data_put_long(data, C.int64_t(v))
	case int:
		if unsafe.Sizeof(0) == 8 {
			C.pn_data_put_long(data, C.int64_t(v))
		} else {
			C.pn_data_put_int(data, C.int32_t(v))
		}
	case uint8:
		C.pn_data_put_ubyte(data, C.uint8_t(v))
	case uint16:
		C.pn_data_put_ushort(data, C.uint16_t(v))
	case uint32:
		C.pn_data_put_uint(data, C.uint32_t(v))
	case uint64:
		C.pn_data_put_ulong(data, C.uint64_t(v))
	case uint:
		if unsafe.Sizeof(0) == 8 {
			C.pn_data_put_ulong(data, C.uint64_t(v))
		} else {
			C.pn_data_put_uint(data, C.uint32_t(v))
		}
	case float32:
		C.pn_data_put_float(data, C.float(v))
	case float64:
		C.pn_data_put_double(data, C.double(v))
	case string:
		C.pn_data_put_string(data, toPnBytes([]byte(v)))
	case []byte:
		C.pn_data_put_binary(data, toPnBytes(v))
	case reflect.Value:
		return marshal(bytesIn, v.Interface())
	default:
		panic(errorf("cannot marshal %s to AMQP", reflect.TypeOf(v)))
	}
	// FIXME aconway 2015-03-11: get size from proton.
	bytes = bytesIn
	for {
		n := int(C.pn_data_encode(data, (*C.char)(unsafe.Pointer(&bytes[0])), C.size_t(cap(bytes))))
		if n != int(C.PN_EOS) {
			if n < 0 {
				err = errorf(pnErrorName(n))
			} else {
				bytes = bytes[0:n]
			}
			return
		}
		bytes = make([]byte, cap(bytes)*2)
	}
	return
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
	e.buffer, err = marshal(e.buffer, v)
	if err == nil {
		e.writer.Write(e.buffer)
	}
	return
}

func toPnBytes(b []byte) C.pn_bytes_t {
	if len(b) == 0 {
		return C.pn_bytes_t{0, nil}
	} else {
		return C.pn_bytes_t{C.size_t(len(b)), (*C.char)(unsafe.Pointer(&b[0]))}
	}
}
