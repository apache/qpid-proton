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

// Utilities to supplement the standard Go testing package
package test

import (
	"fmt"
	"path"
	"reflect"
	"runtime"
	"testing"
)

// The testing.TB.Helper() function does not seem to work
func decorate(msg string, callDepth int) string {
	_, file, line, _ := runtime.Caller(callDepth + 1) // annotate with location of caller.
	_, file = path.Split(file)
	return fmt.Sprintf("\n%s:%d: %v", file, line, msg)
}

func message(err error, args ...interface{}) (msg string) {
	if len(args) > 0 {
		msg = fmt.Sprintf(args[0].(string), args[1:]...) + ": "
	}
	msg = msg + err.Error()
	return
}

// ErrorIf calls t.Error() if err != nil, with optional format message
func ErrorIf(t testing.TB, err error, format ...interface{}) error {
	t.Helper()
	if err != nil {
		t.Error(message(err, format...))
	}
	return err
}

// ErrorIf calls t.Fatal() if err != nil, with optional format message
func FatalIf(t testing.TB, err error, format ...interface{}) {
	t.Helper()
	if err != nil {
		t.Fatal(message(err, format...))
	}
}

// Extend the methods on testing.TB
type TB struct{ testing.TB }

func (t *TB) ErrorIf(err error, format ...interface{}) error {
	t.Helper()
	return ErrorIf(t, err, format...)
}
func (t *TB) FatalIf(err error, format ...interface{}) { t.Helper(); FatalIf(t, err, format...) }

// if reflect.DeepEqual(want, got) { return nil } else { return error("want != got" })
func Differ(want interface{}, got interface{}) error {
	if !reflect.DeepEqual(want, got) {
		return fmt.Errorf("(%T)%#v != (%T)%#v)", want, want, got, got)
	}
	return nil
}
