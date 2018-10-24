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

// The testing.TB.Helper() is only available from Go 1.9.
// Decorate messages with the correct file:line at callDepth before calling
// before calling a testing.TB method.
func decorate(method func(arg ...interface{}), callDepth int, msg string) {
	_, file, line, _ := runtime.Caller(callDepth + 1)
	_, file = path.Split(file)
	method(fmt.Sprintf("\n%s:%d: %v", file, line, msg))
}

// Format a message and call method if err != nil
func checkErr(method func(arg ...interface{}), callDepth int, err error, format ...interface{}) error {
	if err != nil {
		var msg string
		if len(format) > 0 {
			msg = fmt.Sprintf("%v: %v", fmt.Sprintf(format[0].(string), format[1:]...), err)
		} else {
			msg = err.Error()
		}
		decorate(method, callDepth+1, msg)
	}
	return err
}

func ErrorIfN(callDepth int, t testing.TB, err error, format ...interface{}) error {
	return checkErr(t.Error, callDepth+1, err, format...)
}

func ErrorIf(t testing.TB, err error, format ...interface{}) error {
	return ErrorIfN(1, t, err, format...)
}

func FatalIfN(callDepth int, t testing.TB, err error, format ...interface{}) {
	checkErr(t.Fatal, callDepth+1, err, format...)
}

func FatalIf(t testing.TB, err error, format ...interface{}) {
	FatalIfN(1, t, err, format...)
}

// if reflect.DeepEqual(want, got) { return nil } else { return error("want != got" })
func Differ(want interface{}, got interface{}) error {
	if !reflect.DeepEqual(want, got) {
		return fmt.Errorf("(%T)%#v != (%T)%#v)", want, want, got, got)
	}
	return nil
}
