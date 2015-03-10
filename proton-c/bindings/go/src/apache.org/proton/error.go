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

package proton

// #include <proton/error.h>
// #include <proton/codec.h>
import "C"

import (
	"fmt"
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

func nonBlank(a, b string) string {
	if a == "" {
		return b
	}
	return a
}

func pnErrorName(code int) string {
	return nonBlank(pnErrorNames[code], "unknown error code")
}

///
// NOTE: pnError has String() and Error() methods.
// The String() method prints the plain error message, the Error() method
// prints the error message with a "proton:" prefix.
// Thus you can format nested error messages with "%s" without getting nested "proton:"
// prefixes but the prefix will be added when the end user uses Error()
// or "%v" on the error value.
//
type pnError string

func (err pnError) String() string { return string(err) }
func (err pnError) Error() string  { return fmt.Sprintf("proton: %s", string(err)) }

// errorf creates an error with a formatted message
func errorf(format string, a ...interface{}) error {
	return pnError(fmt.Sprintf(format, a...))
}

func pnDataError(data *C.pn_data_t) string {
	err := C.pn_data_error(data)
	if err != nil && int(C.pn_error_code(err)) != 0 {
		return C.GoString(C.pn_error_text(err))
	}
	return ""
}
