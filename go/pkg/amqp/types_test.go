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
	"time"

	"github.com/apache/qpid-proton/go/pkg/internal/test"
)

func checkUnmarshal(marshaled []byte, v interface{}) error {
	got, err := Unmarshal(marshaled, v)
	if err != nil {
		return err
	}
	if got != len(marshaled) {
		return fmt.Errorf("Wanted to Unmarshal %v bytes, got %v", len(marshaled), got)
	}
	return nil
}

func ExampleAnnotationKey() {
	var k AnnotationKey = AnnotationKeySymbol(Symbol("foo"))
	fmt.Println(k.Get().(Symbol))
	k = AnnotationKeyUint64(42)
	fmt.Println(k.Get().(uint64))
	// Output:
	// foo
	// 42
}

var timeValue = time.Now().Round(time.Millisecond)

// Values that are unchanged by a marshal/unmarshal round-trip from interface{}
// to interface{}
var rtValues = []interface{}{
	true,
	int8(-8), int16(-16), int32(-32), int64(-64),
	uint8(8), uint16(16), uint32(32), uint64(64),
	float32(0.32), float64(0.64),
	"string", Binary("Binary"), Symbol("symbol"),
	nil,
	Described{"D", "V"},
	timeValue,
	UUID{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16},
	Char('a'),
	Map{"V": "X"},
	Map{},
	List{"V", int32(1)},
	List{},
	[]string{"a", "b", "c"},
	[]int8{},
	AnyMap{{[]int8{1, 2, 3}, "bad-key"}, {int16(1), "duplicate-1"}, {int16(1), "duplicate-2"}},
}

// Go values that round-trip if unmarshalled back to the same type they were
// marshalled from, but unmarshal to interface{} as a different default type.
var oddValues = []interface{}{
	int(-99),                  // int32|64 depending on platform
	uint(99),                  // int32|64 depending on platform
	[]byte("byte"),            // amqp.Binary
	map[string]int{"str": 99}, // amqp.Map
	[]Map{Map{}},              // amqp.Array - the generic array
	AnyMap(nil),               // Map
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
	"{D V}",
	fmt.Sprintf("%v", timeValue),
	"UUID(01020304-0506-0708-090a-0b0c0d0e0f10)",
	fmt.Sprintf("%v", 'a'),
	"map[V:X]",
	"map[]",
	"[V 1]",
	"[]",
	"[a b c]",
	"[]",
	"[{[1 2 3] bad-key} {1 duplicate-1} {1 duplicate-2}]",
	// for oddValues
	"-99", "99",
	"[98 121 116 101]", /*"byte"*/
	"map[str:99]",
	"[map[]]",
	"[]",
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
		if err := test.Differ(x, v); err != nil {
			t.Error(err)
		}
	}
}

// Round trip from T to T where T is the type of the value.
func TestTypesRoundTripAll(t *testing.T) {
	for _, x := range allValues {
		marshaled, err := Marshal(x, nil)
		if err != nil {
			t.Error(err)
		}
		if x == nil { // We can't create an instance of nil to unmarshal to.
			continue
		}
		vp := reflect.New(reflect.TypeOf(x)) // v points to a Zero of the same type as x
		if err := checkUnmarshal(marshaled, vp.Interface()); err != nil {
			t.Error(err)
		}
		v := vp.Elem().Interface()
		if err := test.Differ(x, v); err != nil {
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
	marshaled, _ := Marshal(want, nil)

	// Unmarshal to Described type
	var d Described
	if err := checkUnmarshal(marshaled, &d); err != nil {
		t.Error(err)
	}
	if err := test.Differ(want, d); err != nil {
		t.Error(err)
	}

	// Unmarshal to interface{}
	var i interface{}
	if err := checkUnmarshal(marshaled, &i); err != nil {
		t.Error(err)
	}
	if _, ok := i.(Described); !ok {
		t.Errorf("Expected Described, got %T(%v)", i, i)
	}
	if err := test.Differ(want, i); err != nil {
		t.Error(err)
	}

	// Unmarshal value only (drop descriptor) to the value type
	var s string
	if err := checkUnmarshal(marshaled, &s); err != nil {
		t.Error(err)
	}
	if err := test.Differ(want.Value, s); err != nil {
		t.Error(err)
	}
}

func TestTimeConversion(t *testing.T) {
	pt := pnTime(timeValue)
	if err := test.Differ(timeValue, goTime(pt)); err != nil {
		t.Error(err)
	}
	if err := test.Differ(pt, pnTime(goTime(pt))); err != nil {
		t.Error(err)
	}
	ut := time.Unix(123, 456*1000000)
	if err := test.Differ(123456, int(pnTime(ut))); err != nil {
		t.Error(err)
	}
	if err := test.Differ(ut, goTime(123456)); err != nil {
		t.Error(err)
	}

	// Preserve zero values
	var tz time.Time
	if err := test.Differ(0, int(pnTime(tz))); err != nil {
		t.Error(err)
	}
	if err := test.Differ(tz, goTime(pnTime(tz))); err != nil {
		t.Error(err)
	}
}
