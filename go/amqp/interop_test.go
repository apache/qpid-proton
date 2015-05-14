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

// Test that conversion of Go type to/from AMQP is compatible with other
// bindings.
//
package amqp

import (
	"bytes"
	"fmt"
	"io"
	"io/ioutil"
	"os"
	"reflect"
	"strings"
	"testing"
)

func assertEqual(want interface{}, got interface{}) {
	if !reflect.DeepEqual(want, got) {
		panic(fmt.Errorf("%#v != %#v", want, got))
	}
}

func assertNil(err interface{}) {
	if err != nil {
		panic(err)
	}
}

func getReader(name string) (r io.Reader) {
	r, err := os.Open("interop/" + name + ".amqp")
	if err != nil {
		panic(fmt.Errorf("Can't open %#v: %v", name, err))
	}
	return
}

func remaining(d *Decoder) string {
	remainder, _ := ioutil.ReadAll(io.MultiReader(d.Buffered(), d.reader))
	return string(remainder)
}

// assertDecode: want is the expected value, gotPtr is a pointer to a
// instance of the same type for Decode.
func assertDecode(d *Decoder, want interface{}, gotPtr interface{}) {

	assertNil(d.Decode(gotPtr))

	got := reflect.ValueOf(gotPtr).Elem().Interface()
	assertEqual(want, got)

	// Try round trip encoding
	bytes, err := Marshal(want, nil)
	assertNil(err)
	n, err := Unmarshal(bytes, gotPtr)
	assertNil(err)
	assertEqual(n, len(bytes))
	got = reflect.ValueOf(gotPtr).Elem().Interface()
	assertEqual(want, got)
}

func TestUnmarshal(t *testing.T) {
	bytes, err := ioutil.ReadAll(getReader("strings"))
	if err != nil {
		t.Error(err)
	}
	for _, want := range []string{"abc\000defg", "abcdefg", "abcdefg", "", "", ""} {
		var got string
		n, err := Unmarshal(bytes, &got)
		if err != nil {
			t.Error(err)
		}
		if want != got {
			t.Errorf("%#v != %#v", want, got)
		}
		bytes = bytes[n:]
	}
}

func TestPrimitivesExact(t *testing.T) {
	d := NewDecoder(getReader("primitives"))
	// Decoding into exact types
	var b bool
	assertDecode(d, true, &b)
	assertDecode(d, false, &b)
	var u8 uint8
	assertDecode(d, uint8(42), &u8)
	var u16 uint16
	assertDecode(d, uint16(42), &u16)
	var i16 int16
	assertDecode(d, int16(-42), &i16)
	var u32 uint32
	assertDecode(d, uint32(12345), &u32)
	var i32 int32
	assertDecode(d, int32(-12345), &i32)
	var u64 uint64
	assertDecode(d, uint64(12345), &u64)
	var i64 int64
	assertDecode(d, int64(-12345), &i64)
	var f32 float32
	assertDecode(d, float32(0.125), &f32)
	var f64 float64
	assertDecode(d, float64(0.125), &f64)
}

func TestPrimitivesCompatible(t *testing.T) {
	d := NewDecoder(getReader("primitives"))
	// Decoding into compatible types
	var b bool
	var i int
	var u uint
	var f float64
	assertDecode(d, true, &b)
	assertDecode(d, false, &b)
	assertDecode(d, uint(42), &u)
	assertDecode(d, uint(42), &u)
	assertDecode(d, -42, &i)
	assertDecode(d, uint(12345), &u)
	assertDecode(d, -12345, &i)
	assertDecode(d, uint(12345), &u)
	assertDecode(d, -12345, &i)
	assertDecode(d, 0.125, &f)
	assertDecode(d, 0.125, &f)
}

// assertDecodeValue: want is the expected value, decode into a reflect.Value
func assertDecodeInterface(d *Decoder, want interface{}) {

	var got, got2 interface{}
	assertNil(d.Decode(&got))

	assertEqual(want, got)

	// Try round trip encoding
	bytes, err := Marshal(got, nil)
	assertNil(err)
	n, err := Unmarshal(bytes, &got2)
	assertNil(err)
	assertEqual(n, len(bytes))
	assertEqual(want, got2)
}

func TestPrimitivesInterface(t *testing.T) {
	d := NewDecoder(getReader("primitives"))
	assertDecodeInterface(d, true)
	assertDecodeInterface(d, false)
	assertDecodeInterface(d, uint8(42))
	assertDecodeInterface(d, uint16(42))
	assertDecodeInterface(d, int16(-42))
	assertDecodeInterface(d, uint32(12345))
	assertDecodeInterface(d, int32(-12345))
	assertDecodeInterface(d, uint64(12345))
	assertDecodeInterface(d, int64(-12345))
	assertDecodeInterface(d, float32(0.125))
	assertDecodeInterface(d, float64(0.125))
}

func TestStrings(t *testing.T) {
	d := NewDecoder(getReader("strings"))
	// Test decoding as plain Go strings
	for _, want := range []string{"abc\000defg", "abcdefg", "abcdefg", "", "", ""} {
		var got string
		assertDecode(d, want, &got)
	}
	remains := remaining(d)
	if remains != "" {
		t.Errorf("leftover: %s", remains)
	}

	// Test decoding as specific string types
	d = NewDecoder(getReader("strings"))
	var bytes []byte
	var str, sym string
	assertDecode(d, []byte("abc\000defg"), &bytes)
	assertDecode(d, "abcdefg", &str)
	assertDecode(d, "abcdefg", &sym)
	assertDecode(d, make([]byte, 0), &bytes)
	assertDecode(d, "", &str)
	assertDecode(d, "", &sym)
	remains = remaining(d)
	if remains != "" {
		t.Fatalf("leftover: %s", remains)
	}

	// Test some error handling
	d = NewDecoder(getReader("strings"))
	var s string
	err := d.Decode(s)
	if err == nil {
		t.Fatal("Expected error")
	}
	if !strings.Contains(err.Error(), "not a pointer") {
		t.Error(err)
	}
	var i int
	err = d.Decode(&i)
	if !strings.Contains(err.Error(), "cannot unmarshal") {
		t.Error(err)
	}
	_, err = Unmarshal([]byte{}, nil)
	if !strings.Contains(err.Error(), "not enough data") {
		t.Error(err)
	}
	_, err = Unmarshal([]byte("foobar"), nil)
	if !strings.Contains(err.Error(), "invalid-argument") {
		t.Error(err)
	}
}

func TestEncodeDecode(t *testing.T) {
	type data struct {
		s  string
		i  int
		u8 uint8
		b  bool
		f  float32
		v  interface{}
	}

	in := data{"foo", 42, 9, true, 1.234, "thing"}

	buf := bytes.Buffer{}
	e := NewEncoder(&buf)
	assertNil(e.Encode(in.s))
	assertNil(e.Encode(in.i))
	assertNil(e.Encode(in.u8))
	assertNil(e.Encode(in.b))
	assertNil(e.Encode(in.f))
	assertNil(e.Encode(in.v))

	var out data
	d := NewDecoder(&buf)
	assertNil(d.Decode(&out.s))
	assertNil(d.Decode(&out.i))
	assertNil(d.Decode(&out.u8))
	assertNil(d.Decode(&out.b))
	assertNil(d.Decode(&out.f))
	assertNil(d.Decode(&out.v))

	assertEqual(in, out)
}

func TestMap(t *testing.T) {
	d := NewDecoder(getReader("maps"))

	// Generic map
	var m Map
	assertDecode(d, Map{"one": int32(1), "two": int32(2), "three": int32(3)}, &m)

	// Interface as map
	var i interface{}
	assertDecode(d, Map{int32(1): "one", int32(2): "two", int32(3): "three"}, &i)

	d = NewDecoder(getReader("maps"))
	// Specific typed map
	var m2 map[string]int
	assertDecode(d, map[string]int{"one": 1, "two": 2, "three": 3}, &m2)

	// Round trip a nested map
	m = Map{int64(1): "one", "two": int32(2), true: Map{uint8(1): true, uint8(2): false}}
	bytes, err := Marshal(m, nil)
	assertNil(err)
	_, err = Unmarshal(bytes, &i)
	assertNil(err)
	assertEqual(m, i)
}

func TestList(t *testing.T) {
	d := NewDecoder(getReader("lists"))
	var l List
	assertDecode(d, List{int32(32), "foo", true}, &l)
	assertDecode(d, List{}, &l)
}

func FIXMETestMessage(t *testing.T) {
	// FIXME aconway 2015-04-09: integrate Message encoding under marshal/unmarshal API.
	bytes, err := ioutil.ReadAll(getReader("message"))
	assertNil(err)
	m, err := DecodeMessage(bytes)
	assertNil(err)
	fmt.Printf("%+v\n", m)
	assertEqual(m.Body(), "hello")

	bytes2 := make([]byte, len(bytes))
	bytes2, err = m.Encode(bytes2)
	assertNil(err)
	assertEqual(bytes, bytes2)
}

// FIXME aconway 2015-03-13: finish the full interop test
