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
import "C"

import (
	"fmt"
)

// errorCode is an error code returned by proton C.
type errorCode int

const (
	errEOS         errorCode = C.PN_EOS
	errError                 = C.PN_ERR
	errOverflow              = C.PN_OVERFLOW
	errUnderflow             = C.PN_UNDERFLOW
	errState                 = C.PN_STATE_ERR
	errArgument              = C.PN_ARG_ERR
	errTimeout               = C.PN_TIMEOUT
	errInterrupted           = C.PN_INTR
	errInProgress            = C.PN_INPROGRESS
)

// String gives a brief description of an errorCode.
func (code errorCode) String() string {
	switch code {
	case errEOS:
		return "end of data"
	case errError:
		return "error"
	case errOverflow:
		return "overflow"
	case errUnderflow:
		return "underflow"
	case errState:
		return "bad state"
	case errArgument:
		return "invalid argument"
	case errTimeout:
		return "timeout"
	case errInterrupted:
		return "interrupted"
	case errInProgress:
		return "in progress"
	}
	return fmt.Sprintf("invalid error code %d", code)
}

// An errorCode can be used as an error
func (code errorCode) Error() string {
	return fmt.Sprintf("proton: %v", code)
}

// pnError is a simple error string.
//
// NOTE: All error types used in proton have both String() and Error() methods.
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
