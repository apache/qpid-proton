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
	"io"
	"io/ioutil"
	"os"
	"reflect"
	"strings"
	"testing"

	"github.com/apache/qpid-proton/go/pkg/internal/test"
)

var skipped = false

func getReader(t *testing.T, name string) (r io.Reader) {
	dir := os.Getenv("PN_INTEROP_DIR")
	if dir == "" {
		if !skipped {
			skipped = true // Don't keep repeating
			t.Skip("no PN_INTEROP_DIR in environment")
		} else {
			t.SkipNow()
		}
	}
	r, err := os.Open(dir + "/" + name + ".amqp")
	if err != nil {
		t.Fatalf("can't open %#v: %v", name, err)
	}
	return
}

func remaining(d *Decoder) string {
	remainder, _ := ioutil.ReadAll(io.MultiReader(d.Buffered(), d.reader))
	return string(remainder)
}

// checkDecode: want is the expected value, gotPtr is a pointer to a
// instance of the same type for Decode.
func checkDecode(d *Decoder, want interface{}, gotPtr interface{}, t *testing.T) {

	if err := d.Decode(gotPtr); err != nil {
		t.Error("Decode failed", err)
		return
	}
	got := reflect.ValueOf(gotPtr).Elem().Interface()
	if err := test.Differ(want, got); err != nil {
		t.Error("Decode bad value:", err)
		return
	}

	// Try round trip encoding
	bytes, err := Marshal(want, nil)
	if err != nil {
		t.Error("Marshal failed", err)
		return
	}
	n, err := Unmarshal(bytes, gotPtr)
	if err != nil {
		t.Error("Unmarshal failed", err)
		return
	}
	err = test.Differ(n, len(bytes))
	if err != nil {
		t.Error("Bad unmarshal length", err)
		return
	}
	got = reflect.ValueOf(gotPtr).Elem().Interface()
	if err = test.Differ(want, got); err != nil {
		t.Error("Bad unmarshal value", err)
		return
	}
}

func TestUnmarshal(t *testing.T) {
	bytes, err := ioutil.ReadAll(getReader(t, "strings"))
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
	d := NewDecoder(getReader(t, "primitives"))
	// Decoding into exact types
	var b bool
	checkDecode(d, true, &b, t)
	checkDecode(d, false, &b, t)
	var u8 uint8
	checkDecode(d, uint8(42), &u8, t)
	var u16 uint16
	checkDecode(d, uint16(42), &u16, t)
	var i16 int16
	checkDecode(d, int16(-42), &i16, t)
	var u32 uint32
	checkDecode(d, uint32(12345), &u32, t)
	var i32 int32
	checkDecode(d, int32(-12345), &i32, t)
	var u64 uint64
	checkDecode(d, uint64(12345), &u64, t)
	var i64 int64
	checkDecode(d, int64(-12345), &i64, t)
	var f32 float32
	checkDecode(d, float32(0.125), &f32, t)
	var f64 float64
	checkDecode(d, float64(0.125), &f64, t)
}

func TestPrimitivesCompatible(t *testing.T) {
	d := NewDecoder(getReader(t, "primitives"))
	// Decoding into compatible types
	var b bool
	var i int
	var u uint
	var f float64
	checkDecode(d, true, &b, t)
	checkDecode(d, false, &b, t)
	checkDecode(d, uint(42), &u, t)
	checkDecode(d, uint(42), &u, t)
	checkDecode(d, -42, &i, t)
	checkDecode(d, uint(12345), &u, t)
	checkDecode(d, -12345, &i, t)
	checkDecode(d, uint(12345), &u, t)
	checkDecode(d, -12345, &i, t)
	checkDecode(d, 0.125, &f, t)
	checkDecode(d, 0.125, &f, t)
}

// checkDecodeValue: want is the expected value, decode into a reflect.Value
func checkDecodeInterface(d *Decoder, want interface{}, t *testing.T) {

	var got, got2 interface{}
	if err := d.Decode(&got); err != nil {
		t.Error("Decode failed", err)
		return
	}
	if err := test.Differ(want, got); err != nil {
		t.Error(err)
		return
	}
	// Try round trip encoding
	bytes, err := Marshal(got, nil)
	if err != nil {
		t.Error(err)
		return
	}
	n, err := Unmarshal(bytes, &got2)
	if err != nil {
		t.Error(err)
		return
	}
	if err := test.Differ(n, len(bytes)); err != nil {
		t.Error(err)
		return
	}
	if err := test.Differ(want, got2); err != nil {
		t.Error(err)
		return
	}
}

func TestPrimitivesInterface(t *testing.T) {
	d := NewDecoder(getReader(t, "primitives"))
	checkDecodeInterface(d, true, t)
	checkDecodeInterface(d, false, t)
	checkDecodeInterface(d, uint8(42), t)
	checkDecodeInterface(d, uint16(42), t)
	checkDecodeInterface(d, int16(-42), t)
	checkDecodeInterface(d, uint32(12345), t)
	checkDecodeInterface(d, int32(-12345), t)
	checkDecodeInterface(d, uint64(12345), t)
	checkDecodeInterface(d, int64(-12345), t)
	checkDecodeInterface(d, float32(0.125), t)
	checkDecodeInterface(d, float64(0.125), t)
}

func TestStrings(t *testing.T) {
	d := NewDecoder(getReader(t, "strings"))
	// Test decoding as plain Go strings
	for _, want := range []string{"abc\000defg", "abcdefg", "abcdefg", "", "", ""} {
		var got string
		checkDecode(d, want, &got, t)
	}
	remains := remaining(d)
	if remains != "" {
		t.Errorf("leftover: %s", remains)
	}

	// Test decoding as specific string types
	d = NewDecoder(getReader(t, "strings"))
	var bytes []byte
	var str, sym string
	checkDecode(d, []byte("abc\000defg"), &bytes, t)
	checkDecode(d, "abcdefg", &str, t)
	checkDecode(d, "abcdefg", &sym, t)
	checkDecode(d, make([]byte, 0), &bytes, t)
	checkDecode(d, "", &str, t)
	checkDecode(d, "", &sym, t)
	remains = remaining(d)
	if remains != "" {
		t.Fatalf("leftover: %s", remains)
	}

	// Test some error handling
	d = NewDecoder(getReader(t, "strings"))
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
	test.ErrorIf(t, test.Differ(err, EndOfData))
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
	if err := e.Encode(in.s); err != nil {
		t.Error(err)
	}
	if err := e.Encode(in.i); err != nil {
		t.Error(err)
	}
	if err := e.Encode(in.u8); err != nil {
		t.Error(err)
	}
	if err := e.Encode(in.b); err != nil {
		t.Error(err)
	}
	if err := e.Encode(in.f); err != nil {
		t.Error(err)
	}
	if err := e.Encode(in.v); err != nil {
		t.Error(err)
	}

	var out data
	d := NewDecoder(&buf)
	if err := d.Decode(&out.s); err != nil {
		t.Error(err)
	}
	if err := d.Decode(&out.i); err != nil {
		t.Error(err)
	}
	if err := d.Decode(&out.u8); err != nil {
		t.Error(err)
	}
	if err := d.Decode(&out.b); err != nil {
		t.Error(err)
	}
	if err := d.Decode(&out.f); err != nil {
		t.Error(err)
	}
	if err := d.Decode(&out.v); err != nil {
		t.Error(err)
	}

	if err := test.Differ(in, out); err != nil {
		t.Error(err)
	}
}

func TestMap(t *testing.T) {
	d := NewDecoder(getReader(t, "maps"))

	// Generic map
	var m Map
	checkDecode(d, Map{"one": int32(1), "two": int32(2), "three": int32(3)}, &m, t)

	// Interface as map
	var i interface{}
	checkDecode(d, Map{int32(1): "one", int32(2): "two", int32(3): "three"}, &i, t)

	d = NewDecoder(getReader(t, "maps"))
	// Specific typed map
	var m2 map[string]int
	checkDecode(d, map[string]int{"one": 1, "two": 2, "three": 3}, &m2, t)

	// Nested map
	m = Map{int64(1): "one", "two": int32(2), true: Map{uint8(1): true, uint8(2): false}}
	bytes, err := Marshal(m, nil)
	if err != nil {
		t.Fatal(err)
	}
	_, err = Unmarshal(bytes, &i)
	if err != nil {
		t.Fatal(err)
	}
	if err = test.Differ(m, i); err != nil {
		t.Fatal(err)
	}
}

func TestList(t *testing.T) {
	d := NewDecoder(getReader(t, "lists"))
	var l List
	checkDecode(d, List{int32(32), "foo", true}, &l, t)
	checkDecode(d, List{}, &l, t)
}

// TODO aconway 2015-09-08: the message.amqp file seems to be incorrectly coded as
// as an AMQP string *inside* an AMQP binary?? Skip the test for now.
func TODO_TestMessage(t *testing.T) {
	bytes, err := ioutil.ReadAll(getReader(t, "message"))
	if err != nil {
		t.Fatal(err)
	}

	m, err := DecodeMessage(bytes)
	if err != nil {
		t.Fatal(err)
	} else {
		if err = test.Differ(m.Body(), "hello"); err != nil {
			t.Error(err)
		}
	}

	m2 := NewMessageWith("hello")
	bytes2, err := m2.Encode(nil)
	if err != nil {
		t.Error(err)
	} else {
		if err = test.Differ(bytes, bytes2); err != nil {
			t.Error(err)
		}
	}
}

// TODO aconway 2015-03-13: finish the full interop test
