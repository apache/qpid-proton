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
package proton

// #cgo LDFLAGS: -lqpid-proton-core
// #include <proton/error.h>
// #include <proton/codec.h>
import "C"

import (
	"fmt"
	"sync"
	"sync/atomic"
)

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

func PnError(e *C.pn_error_t) error {
	if e == nil || C.pn_error_code(e) == 0 {
		return nil
	}
	return fmt.Errorf("%s: %s", PnErrorCode(C.pn_error_code(e)), C.GoString(C.pn_error_text(e)))
}

// ErrorHolder is a goroutine-safe error holder that keeps the first error that is set.
type ErrorHolder struct {
	once  sync.Once
	value atomic.Value
}

// Set the error if not already set
func (e *ErrorHolder) Set(err error) {
	if err != nil {
		e.once.Do(func() { e.value.Store(err) })
	}
}

// Get the error.
func (e *ErrorHolder) Get() (err error) {
	err, _ = e.value.Load().(error)
	return
}
