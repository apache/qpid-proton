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

package amqp

import (
	"fmt"
	"reflect"
	"testing"
)

func checkEqual(want interface{}, got interface{}) error {
	if !reflect.DeepEqual(want, got) {
		return fmt.Errorf("%#v != %#v", want, got)
	}
	return nil
}

func checkUnmarshal(marshalled []byte, v interface{}) error {
	got, err := Unmarshal(marshalled, v)
	if err != nil {
		return err
	}
	if got != len(marshalled) {
		return fmt.Errorf("Wanted to Unmarshal %v bytes, got %v", len(marshalled), got)
	}
	return nil
}

func ExampleKey() {
	var k AnnotationKey = AnnotationKeySymbol(Symbol("foo"))
	fmt.Println(k.Get().(Symbol))
	k = AnnotationKeyUint64(42)
	fmt.Println(k.Get().(uint64))
	// Output:
	// foo
	// 42
}

// Values that are unchanged by a marshal/unmarshal round-trip from interface{}
// to interface{}
var rtValues = []interface{}{
	true,
	int8(-8), int16(-16), int32(-32), int64(-64),
	uint8(8), uint16(16), uint32(32), uint64(64),
	float32(0.32), float64(0.64),
	"string", Binary("Binary"), Symbol("symbol"),
	nil,
	Map{"V": "X"},
	List{"V", int32(1)},
	Described{"D", "V"},
}

// Go values that unmarshal as an equivalent value but a different type
// if unmarshalled to interface{}.
var oddValues = []interface{}{
	int(-99), uint(99), // [u]int32|64
	[]byte("byte"),            // amqp.Binary
	map[string]int{"str": 99}, // amqp.Map
	[]string{"a", "b"},        // amqp.List
}

var allValues = append(rtValues, oddValues...)

// %v formatted representation of allValues
var vstrings = []string{
	// for rtValues
	"true",
	"-8", "-16", "-32", "-64",
	"8", "16", "32", "64",
	"0.32", "0.64",
	"string", "Binary", "symbol",
	"<nil>",
	"map[V:X]",
	"[V 1]",
	"{D V}",
	// for oddValues
	"-99", "99",
	"[98 121 116 101]", /*"byte"*/
	"map[str:99]",
	"[a b]",
}

// Round-trip encoding test
func TestTypesRoundTrip(t *testing.T) {
	for _, x := range rtValues {
		marshalled, err := Marshal(x, nil)
		if err != nil {
			t.Error(err)
		}
		var v interface{}
		if err := checkUnmarshal(marshalled, &v); err != nil {
			t.Error(err)
		}
		if err := checkEqual(v, x); err != nil {
			t.Error(t, err)
		}
	}
}

// Round trip from T to T where T is the type of the value.
func TestTypesRoundTripAll(t *testing.T) {
	for _, x := range allValues {
		marshalled, err := Marshal(x, nil)
		if err != nil {
			t.Error(err)
		}
		if x == nil { // We can't create an instance of nil to unmarshal to.
			continue
		}
		vp := reflect.New(reflect.TypeOf(x)) // v points to a Zero of the same type as x
		if err := checkUnmarshal(marshalled, vp.Interface()); err != nil {
			t.Error(err)
		}
		v := vp.Elem().Interface()
		if err := checkEqual(v, x); err != nil {
			t.Error(err)
		}
	}
}

func TestTypesPrint(t *testing.T) {
	// Default %v representations of rtValues and oddValues
	for i, x := range allValues {
		if s := fmt.Sprintf("%v", x); vstrings[i] != s {
			t.Errorf("printing %T: want %v, got %v", x, vstrings[i], s)
		}
	}
}

func TestDescribed(t *testing.T) {
	want := Described{"D", "V"}
	marshalled, _ := Marshal(want, nil)

	// Unmarshal to Described type
	var d Described
	if err := checkUnmarshal(marshalled, &d); err != nil {
		t.Error(err)
	}
	if err := checkEqual(want, d); err != nil {
		t.Error(err)
	}

	// Unmarshal to interface{}
	var i interface{}
	if err := checkUnmarshal(marshalled, &i); err != nil {
		t.Error(err)
	}
	if _, ok := i.(Described); !ok {
		t.Errorf("Expected Described, got %T(%v)", i, i)
	}
	if err := checkEqual(want, i); err != nil {
		t.Error(err)
	}

	// Unmarshal value only (drop descriptor) to the value type
	var s string
	if err := checkUnmarshal(marshalled, &s); err != nil {
		t.Error(err)
	}
	if err := checkEqual(want.Value, s); err != nil {
		t.Error(err)
	}

	// Nested described types
	want = Described{Described{int64(123), true}, "foo"}
	marshalled, _ = Marshal(want, nil)
	if err := checkUnmarshal(marshalled, &d); err != nil {
		t.Error(err)
	}
	if err := checkEqual(want, d); err != nil {
		t.Error(err)
	}
	// Nested to interface
	if err := checkUnmarshal(marshalled, &i); err != nil {
		t.Error(err)
	}
	if err := checkEqual(want, i); err != nil {
		t.Error(err)
	}
}
