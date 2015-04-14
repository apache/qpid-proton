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

package event // FIXME aconway 2015-03-26: duplicated from package proton, clean up

// #include <proton/error.h>
// #include <proton/codec.h>
import "C"

import (
	"fmt"
	"runtime"
)

var pnErrorNames = map[int]string{
	C.PN_EOS:        "end of data",
	C.PN_ERR:        "error",
	C.PN_OVERFLOW:   "overflow",
	C.PN_UNDERFLOW:  "underflow",
	C.PN_STATE_ERR:  "bad state",
	C.PN_ARG_ERR:    "invalid argument",
	C.PN_TIMEOUT:    "timeout",
	C.PN_INTR:       "interrupted",
	C.PN_INPROGRESS: "in progress",
}

func pnErrorName(code int) string {
	name := pnErrorNames[code]
	if name != "" {
		return name
	} else {
		return "unknown"
	}
}

func pnError(e *C.pn_error_t) error {
	if e == nil || C.pn_error_code(e) == 0 {
		return nil
	}
	return errorf("%s: %s", pnErrorName(int(C.pn_error_code(e))), C.GoString(C.pn_error_text(e)))
}

// errorf creates an error with a formatted message
func errorf(format string, a ...interface{}) error {
	return fmt.Errorf("proton: %s", fmt.Sprintf(format, a...))
}

// doRecover is called to recover from internal panics
func doRecover(err *error) {
	r := recover()
	switch r := r.(type) {
	case nil:
		return
	case runtime.Error:
		panic(r)
	case error:
		*err = r
	default:
		panic(r)
	}
}
