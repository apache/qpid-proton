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
// FIXME aconway 2015-03-01: this should move to proton/tests/go when we integrate
// better with the proton build system.
//
package proton

import (
	"bytes"
	"io"
	"io/ioutil"
	"os"
	"reflect"
	"strings"
	"testing"
)

func assertEqual(want interface{}, got interface{}) {
	if !reflect.DeepEqual(want, got) {
		panic(errorf("%T(%v) != %T(%v)", want, want, got, got))
	}
}

func assertNil(err interface{}) {
	if err != nil {
		panic(err)
	}
}

func getReader(name string) (r io.Reader) {
	r, err := os.Open("../../../../../../tests/interop/" + name + ".amqp")
	if err != nil {
		panic(errorf("Can't open %#v: %v", name, err))
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
	bytes, err := Marshal(want)
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
func assertDecodeValue(d *Decoder, want interface{}) {

	var v reflect.Value
	assertNil(d.Decode(&v))

	got := v.Interface()
	assertEqual(want, got)

	// Try round trip encoding
	bytes, err := Marshal(v)
	assertNil(err)
	n, err := Unmarshal(bytes, &v)
	assertNil(err)
	assertEqual(n, len(bytes))
	got = v.Interface()
	assertEqual(want, got)
}

func TestPrimitivesValue(t *testing.T) {
	d := NewDecoder(getReader("primitives"))
	// Decoding into reflect.Value
	assertDecodeValue(d, true)
	assertDecodeValue(d, false)
	assertDecodeValue(d, uint8(42))
	assertDecodeValue(d, uint16(42))
	assertDecodeValue(d, int16(-42))
	assertDecodeValue(d, uint32(12345))
	assertDecodeValue(d, int32(-12345))
	assertDecodeValue(d, uint64(12345))
	assertDecodeValue(d, int64(-12345))
	assertDecodeValue(d, float32(0.125))
	assertDecodeValue(d, float64(0.125))
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
		panic(errorf("leftover: %s", remains))
	}

	// Test some error handling
	d = NewDecoder(getReader("strings"))
	var s string
	err := d.Decode(s)
	if !strings.Contains(err.Error(), "not a pointer") {
		t.Error(err)
	}
	var i int
	err = d.Decode(&i)
	if !strings.Contains(err.Error(), "cannot unmarshal") {
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
	}

	in := data{"foo", 42, 9, true, 1.234}

	buf := bytes.Buffer{}
	e := NewEncoder(&buf)
	e.Encode(in.s)
	e.Encode(in.i)
	e.Encode(in.u8)
	e.Encode(in.b)
	e.Encode(in.f)

	var out data
	d := NewDecoder(&buf)
	d.Decode(&out.s)
	d.Decode(&out.i)
	d.Decode(&out.u8)
	d.Decode(&out.b)
	d.Decode(&out.f)

	assertEqual(in, out)

	vIn := reflect.ValueOf("thing")
	e.Encode(vIn)
	var vOut reflect.Value
	d.Decode(&vOut)
	assertEqual("thing", vOut.Interface())
}

func BenchmarkDecode(b *testing.B) {
	var buf bytes.Buffer
	for _, f := range []string{"strings", "primitives"} {
		_, err := buf.ReadFrom(getReader(f))
		if err != nil {
			panic(err)
		}
	}

	d := NewDecoder(bytes.NewReader(buf.Bytes()))

	decode := func(v interface{}) {
		err := d.Decode(v)
		if err != nil {
			panic(err)
		}
	}

	for i := 0; i < b.N; i++ {
		var by []byte
		// strings
		decode(&by)
		var s string
		decode(&s)
		decode(&s)
		decode(&by)
		decode(&s)
		decode(&s)
		// primitives
		var b bool
		decode(&b)
		decode(&b)
		var u8 uint8
		decode(&u8)
		var u16 uint16
		decode(&u16)
		var i16 int16
		decode(&i16)
		var u32 uint32
		decode(&u32)
		var i32 int32
		decode(&i32)
		var u64 uint64
		decode(&u64)
		var i64 int64
		decode(&i64)
		var f32 float32
		decode(&f32)
		var f64 float64
		decode(&f64)

		d = NewDecoder(bytes.NewReader(buf.Bytes()))
	}
}

func BenchmarkEncode(b *testing.B) {

	var buf bytes.Buffer
	buf.Grow(10000) // Avoid buffer reallocation during benchmark
	e := NewEncoder(&buf)
	encode := func(v interface{}) {
		err := e.Encode(v)
		if err != nil {
			panic(err)
		}
	}

	for i := 0; i < b.N; i++ {
		// strings
		encode([]byte("foo"))
		encode("foo")
		encode("bar")
		encode([]byte(""))
		encode("")
		encode("")
		// primitives
		encode(true)
		encode(false)
		encode(uint8(42))
		encode(int8(-42))
		encode(uint16(12345))
		encode(int16(-12345))
		encode(uint32(123453245))
		encode(int32(-123453245))
		encode(uint64(123456445))
		encode(int64(-123456445))
		encode(float32(1.2345))
		encode(float64(1.23456))
	}
}
