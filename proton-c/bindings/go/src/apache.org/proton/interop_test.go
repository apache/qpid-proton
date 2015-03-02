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
	"fmt"
	"io"
	"io/ioutil"
	"os"
	"reflect"
	"testing"
)

func getReader(t *testing.T, name string) (r io.Reader) {
	r, err := os.Open("../../../../../../tests/interop/" + name + ".amqp")
	if err != nil {
		t.Fatalf("Can't open %#v: %v", name, err)
	}
	return
}

func remaining(d *Decoder) string {
	remainder, _ := ioutil.ReadAll(io.MultiReader(d.Buffered(), d.reader))
	return string(remainder)
}

// Expectation of a test, want is the expected value, got is a pointer to a
// instance of the same type, which will be replaced by Decode.
type expect struct {
	want, got interface{}
}

// checkDecode: want is the expected value, gotPtr is a pointer to a
// instance of the same type for Decode.
func checkDecode(d *Decoder, want interface{}, gotPtr interface{}) error {
	err := d.Decode(gotPtr)
	if err != nil {
		return err
	}
	got := reflect.ValueOf(gotPtr).Elem().Interface()
	if !reflect.DeepEqual(want, got) {
		return fmt.Errorf("%#v != %#v", want, got)
	}
	return nil
}

func TestUnmarshal(t *testing.T) {
	bytes, err := ioutil.ReadAll(getReader(t, "strings"))
	if err != nil {
		t.Error(err)
	}
	var got string
	err = Unmarshal(bytes, &got)
	if err != nil {
		t.Error(err)
	}
	want := "abc\000defg"
	if want != got {
		t.Errorf("%#v != %#v", want, got)
	}
}

func TestStrings(t *testing.T) {
	d := NewDecoder(getReader(t, "strings"))
	// Test decoding as plain Go strings
	for i, want := range []string{"abc\000defg", "abcdefg", "abcdefg", "", "", ""} {
		var got string
		if err := checkDecode(d, want, &got); err != nil {
			t.Errorf("%d: %v", i, err)
		}
	}
	remains := remaining(d)
	if remains != "" {
		t.Errorf("leftover: %s", remains)
	}

	// Test decoding as specific string types
	d = NewDecoder(getReader(t, "strings"))
	var bytes []byte
	var str string
	var sym Symbol
	for i, expect := range []expect{
		{[]byte("abc\000defg"), &bytes},
		{"abcdefg", &str},
		{Symbol("abcdefg"), &sym},
		{make([]byte, 0), &bytes},
		{"", &str},
		{Symbol(""), &sym},
	} {
		if err := checkDecode(d, expect.want, expect.got); err != nil {
			t.Errorf("%d: %v", i, err)
		}
	}
	remains = remaining(d)
	if remains != "" {
		t.Errorf("leftover: %s", remains)
	}
}
