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

//#include <proton/codec.h>
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

type pnDecoder struct{ data *C.pn_data_t }

func newPnDecoder() pnDecoder { return pnDecoder{C.pn_data(0)} }
func (pd pnDecoder) free()    { C.pn_data_free((*C.pn_data_t)(pd.data)) }

// decode from bytes. Return bytes decoded and errEOS if we run out of data.
func (pd pnDecoder) decode(bytes []byte) (n int, err error) {
	C.pn_data_clear(pd.data)
	if len(bytes) == 0 {
		return 0, errEOS
	}
	cBuf := (*C.char)(unsafe.Pointer(&bytes[0]))
	result := int(C.pn_data_decode(pd.data, cBuf, C.size_t(len(bytes))))
	if result < 0 {
		return 0, errorCode(result)
	} else {
		return result, nil
	}
}

// Unmarshal decodes bytes and converts into the value pointed to by v.
//
// Returns the number of bytes decoded and an errorCode on error.
func (pd pnDecoder) unmarshal(bytes []byte, v interface{}) (n int, err error) {
	n, err = pd.decode(bytes)
	if err != nil {
		return
	}
	switch v := v.(type) {
	case *string:
		err = pd.unmarshalString(v)
	case *[]byte:
		err = pd.unmarshalBytes(v)
	case *Symbol:
		err = pd.unmarshalSymbol(v)
	default:
		note := ""
		if reflect.TypeOf(v).Kind() != reflect.Ptr {
			note = "is not a pointer"
		}
		return 0, errorf("Unmarshal bad type: %T %s", v, note)
		// FIXME aconway 2015-03-02: not finished
	}
	if err != nil {
		return 0, err
	}
	return
}

func (pd pnDecoder) unmarshalPnBytes(target string) (pnBytes C.pn_bytes_t, err error) {
	switch amqpType := C.pn_data_type(pd.data); amqpType {
	case C.PN_STRING:
		pnBytes = C.pn_data_get_string(pd.data)
	case C.PN_BINARY:
		pnBytes = C.pn_data_get_binary(pd.data)
	case C.PN_SYMBOL:
		pnBytes = C.pn_data_get_symbol(pd.data)
	default:
		// FIXME aconway 2015-03-02: error message - json style UnmarsalTypeError?
		return C.pn_bytes_t{}, errorf("Unmarshal cannot convert %#v to %s", amqpType, target)
	}
	return pnBytes, nil
}

func (pd pnDecoder) unmarshalString(v *string) error {
	pnBytes, err := pd.unmarshalPnBytes("string")
	if err == nil {
		*v = C.GoStringN(pnBytes.start, C.int(pnBytes.size))
	}
	return err
}

func (pd pnDecoder) unmarshalBytes(v *[]byte) error {
	pnBytes, err := pd.unmarshalPnBytes("[]byte")
	*v = C.GoBytes(unsafe.Pointer(pnBytes.start), C.int(pnBytes.size))
	return err
}

func (pd pnDecoder) unmarshalSymbol(v *Symbol) error {
	pnBytes, err := pd.unmarshalPnBytes("symbol")
	if err == nil {
		*v = Symbol(C.GoStringN(pnBytes.start, C.int(pnBytes.size)))
	}
	return err
}

/*
Unmarshal decodes AMQP-encoded bytes and stores the result in the value pointed to by v.

FIXME mapping details

 +-------------------------------+-----------------------------------------------+
 |AMQP type                      |Go type                                        |
 +-------------------------------+-----------------------------------------------+
 |string                         |string                                         |
 +-------------------------------+-----------------------------------------------+
 |symbol                         |proton.Symbol                                  |
 +-------------------------------+-----------------------------------------------+
 |binary                         |[]byte                                         |
 +-------------------------------+-----------------------------------------------+
*/
func Unmarshal(bytes []byte, v interface{}) error {
	pd := newPnDecoder()
	defer pd.free()
	_, err := pd.unmarshal(bytes, v)
	return err
}

// Decoder decodes AMQP values from an io.Reader.
//
type Decoder struct {
	reader  io.Reader
	buffer  bytes.Buffer
	readErr error // Outstanding error on our reader
}

// NewDecoder returns a new decoder that reads from r.
//
// The decoder has it's own buffer and may read more data than required for the
// AMQP values requested.  Use Buffered to see if there is data left in the
// buffer.
//
func NewDecoder(r io.Reader) *Decoder {
	return &Decoder{r, bytes.Buffer{}, nil}
}

// Buffered returns a reader of the data remaining in the Decoder's buffer. The
// reader is valid until the next call to Decode.
//
func (d *Decoder) Buffered() io.Reader {
	return bytes.NewReader(d.buffer.Bytes())
}

// more reads more data when we can't parse a complete AMQP type
func (d *Decoder) more() error {
	if d.readErr != nil { // Reader already broken, give up
		return d.readErr
	}
	var readSize int64 = 256
	if int64(d.buffer.Len()) > readSize { // Grow by doubling
		readSize = int64(d.buffer.Len())
	}
	var n int64
	n, d.readErr = d.buffer.ReadFrom(&io.LimitedReader{d.reader, readSize})
	if n == 0 { // ReadFrom won't report io.EOF, just returns 0
		if d.readErr != nil {
			return d.readErr
		} else {
			return errorf("no data")
		}
	}
	return nil
}

// Decode reads the next AMQP value from the Reader and stores it in the value pointed to by v.
//
// See the documentation for Unmarshal for details about the conversion of AMQP into a Go value.
//
func (d *Decoder) Decode(v interface{}) (err error) {
	pd := newPnDecoder()
	defer pd.free()

	// On errEOS, read more data and try again till we have a complete pn_data.
	for {
		var n int
		n, err = pd.unmarshal(d.buffer.Bytes(), v)
		switch err {
		case nil:
			d.buffer.Next(n)
			return
		case errEOS:
			err = d.more()
			if err != nil {
				return err
			}
		default:
			return err
		}
	}
	return err
}
