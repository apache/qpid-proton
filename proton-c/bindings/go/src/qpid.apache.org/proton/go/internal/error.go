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

// Internal implementation details - ignore.
package internal

// #cgo LDFLAGS: -lqpid-proton
// #include <proton/error.h>
// #include <proton/codec.h>
import "C"

import (
	"fmt"
	"runtime"
	"sync"
	"unsafe"
)

// Error type for all proton errors.
type Error string

// Error prefixes error message with proton:
func (e Error) Error() string {
	return "proton: " + string(e)
}

// Errorf creates an Error with a formatted message
func Errorf(format string, a ...interface{}) Error {
	return Error(fmt.Sprintf(format, a...))
}

type PnErrorCode int

func (e PnErrorCode) String() string {
	switch e {
	case C.PN_EOS:
		return "end-of-data"
	case C.PN_ERR:
		return "error"
	case C.PN_OVERFLOW:
		return "overflow"
	case C.PN_UNDERFLOW:
		return "underflow"
	case C.PN_STATE_ERR:
		return "bad-state"
	case C.PN_ARG_ERR:
		return "invalid-argument"
	case C.PN_TIMEOUT:
		return "timeout"
	case C.PN_INTR:
		return "interrupted"
	case C.PN_INPROGRESS:
		return "in-progress"
	default:
		return fmt.Sprintf("unknown-error(%d)", e)
	}
}

func PnError(p unsafe.Pointer) error {
	e := (*C.pn_error_t)(p)
	if e == nil || C.pn_error_code(e) == 0 {
		return nil
	}
	return Errorf("%s: %s", PnErrorCode(C.pn_error_code(e)), C.GoString(C.pn_error_text(e)))
}

// DoRecover is called to recover from internal panics
func DoRecover(err *error) {
	r := recover()
	switch r := r.(type) {
	case nil: // We are not recovering
		return
	case runtime.Error: // Don't catch runtime.Error
		panic(r)
	case error:
		*err = r
	default:
		panic(r)
	}
}

// panicIf panics if condition is true, the panic value is Errorf(fmt, args...)
func panicIf(condition bool, fmt string, args ...interface{}) {
	if condition {
		panic(Errorf(fmt, args...))
	}
}

// FirstError is a goroutine-safe error holder that keeps the first error that is set.
type FirstError struct {
	err  error
	lock sync.Mutex
}

// Set the error if not already set, return the error.
func (e *FirstError) Set(err error) error {
	e.lock.Lock()
	defer e.lock.Unlock()
	if e.err == nil {
		e.err = err
	}
	return e.err
}

// Get the error.
func (e *FirstError) Get() error {
	e.lock.Lock()
	defer e.lock.Unlock()
	return e.err
}
