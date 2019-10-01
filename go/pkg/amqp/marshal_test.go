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
	"strings"
	"testing"

	"github.com/apache/qpid-proton/go/pkg/internal/test"
)

func TestSymbolKey(t *testing.T) {
	bytes, err := Marshal(AnnotationKeySymbol("foo"), nil)
	test.FatalIf(t, err)
	var k AnnotationKey
	_, err = Unmarshal(bytes, &k)
	test.ErrorIf(t, err)
	test.ErrorIf(t, test.Differ("foo", string(k.Get().(Symbol))))
	var sym Symbol
	_, err = Unmarshal(bytes, &sym)
	test.ErrorIf(t, err)
	test.ErrorIf(t, test.Differ("foo", sym.String()))
}

func TestStringKey(t *testing.T) {
	bytes, err := Marshal(AnnotationKeyString("foo"), nil)
	test.FatalIf(t, err)
	var k AnnotationKey

	_, err = Unmarshal(bytes, &k)
	test.ErrorIf(t, err)
	test.ErrorIf(t, test.Differ("foo", string(k.Get().(Symbol))))
	var s string
	_, err = Unmarshal(bytes, &s)
	test.ErrorIf(t, err)
	test.ErrorIf(t, test.Differ("foo", s))
}

func TestIntKey(t *testing.T) {
	bytes, err := Marshal(AnnotationKeyUint64(12345), nil)
	if err != nil {
		t.Fatal(err)
	}
	var k AnnotationKey
	if _, err := Unmarshal(bytes, &k); err != nil {
		t.Error(err)
	}
	if 12345 != k.Get().(uint64) {
		t.Errorf("%v != %v", 12345, k.Get().(uint64))
	}
	var n uint64
	if _, err := Unmarshal(bytes, &n); err != nil {
		t.Error(err)
	}
	if 12345 != n {
		t.Errorf("%v != %v", 12345, k.Get().(uint64))
	}

}

func TestMapToMap(t *testing.T) {
	in := Map{"k": "v", "x": "y", true: false, int8(3): uint64(24)}
	if bytes, err := Marshal(in, nil); err == nil {
		var out Map
		if _, err := Unmarshal(bytes, &out); err == nil {
			if err = test.Differ(in, out); err != nil {
				t.Error(err)
			}
		} else {
			t.Error(err)
		}
	}
}

func TestMapToInterface(t *testing.T) {
	in := Map{"k": "v", "x": "y", true: false, int8(3): uint64(24)}
	if bytes, err := Marshal(in, nil); err == nil {
		var out interface{}
		if _, err := Unmarshal(bytes, &out); err == nil {
			if err = test.Differ(in, out); err != nil {
				t.Error(err)
			}
		} else {
			t.Error(err)
		}
	}
}

func TestAnyMap(t *testing.T) {
	// nil
	bytes, err := Marshal(AnyMap(nil), nil)
	if err != nil {
		t.Error(err)
	}
	var out AnyMap
	if _, err = Unmarshal(bytes, &out); err != nil {
		t.Error(err)
	}
	if err = test.Differ(AnyMap(nil), out); err != nil {
		t.Error(err)
	}

	// empty
	bytes, err = Marshal(AnyMap{}, nil)
	if err != nil {
		t.Error(err)
	}
	if _, err = Unmarshal(bytes, &out); err != nil {
		t.Error(err)
	}
	if err = test.Differ(AnyMap(nil), out); err != nil {
		t.Error(err)
	}

	// with data
	in := AnyMap{{"k", "v"}, {true, false}}
	bytes, err = Marshal(in, nil)
	if err != nil {
		t.Error(err)
	}
	if _, err = Unmarshal(bytes, &out); err != nil {
		t.Error(err)
	}
	if err = test.Differ(in, out); err != nil {
		t.Error(err)
	}
}

func TestBadMap(t *testing.T) {
	// unmarshal map with invalid keys
	in := AnyMap{{"k", "v"}, {[]string{"x", "y"}, "invalid-key"}}
	bytes, err := Marshal(in, nil)
	if err != nil {
		t.Error(err)
	}
	m := Map{}
	//  Should fail to unmarshal to a map
	if _, err = Unmarshal(bytes, &m); err != nil {
		if !strings.Contains(err.Error(), "key []string{\"x\", \"y\"} is not comparable") {
			t.Error(err)
		}
	} else {
		t.Error("expected error")
	}
	// Should unmarshal to an AnyMap
	var out AnyMap
	if _, err = Unmarshal(bytes, &out); err != nil {
		t.Error(err)
	} else if err = test.Differ(in, out); err != nil {
		t.Error(err)
	}
	// Should unmarshal to interface{} as AnyMap
	var v interface{}
	if _, err = Unmarshal(bytes, &v); err != nil {
		t.Error(err)
	} else if err = test.Differ(in, v); err != nil {
		t.Error(err)
	}
	// Round trip from interface to interface
	in = AnyMap{{[]int8{1, 2, 3}, "bad-key"}, {int16(1), "duplicate-1"}, {int16(1), "duplicate-2"}}
	bytes, err = Marshal(interface{}(in), nil)
	if err != nil {
		t.Error(err)
	}
	v = nil
	if _, err = Unmarshal(bytes, &v); err != nil {
		t.Error(err)
	} else if err = test.Differ(in, v); err != nil {
		t.Error(err)
	}
}
