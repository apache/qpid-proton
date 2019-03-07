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

// #include <proton/error.h>
import "C"

import (
	"fmt"
)

// Error is an AMQP error condition. It has a name and a description.
// It implements the Go error interface so can be returned as an error value.
//
// You can pass amqp.Error to methods that send an error to a remote endpoint,
// this gives you full control over what the remote endpoint will see.
//
// You can also pass any Go error to such functions, the remote peer
// will see the equivalent of MakeError(error)
//
type Error struct{ Name, Description string }

// Error implements the Go error interface for AMQP error errors.
func (c Error) Error() string { return fmt.Sprintf("%s: %s", c.Name, c.Description) }

// Errorf makes a Error with name and formatted description as per fmt.Sprintf
func Errorf(name, format string, arg ...interface{}) Error {
	return Error{name, fmt.Sprintf(format, arg...)}
}

// MakeError makes an AMQP error from a go error: {Name: InternalError, Description: err.Error()}
// If err is already an amqp.Error it is returned unchanged.
func MakeError(err error) Error {
	if amqpErr, ok := err.(Error); ok {
		return amqpErr
	} else {
		return Error{InternalError, err.Error()}
	}
}

var (
	InternalError         = "amqp:internal-error"
	NotFound              = "amqp:not-found"
	UnauthorizedAccess    = "amqp:unauthorized-access"
	DecodeError           = "amqp:decode-error"
	ResourceLimitExceeded = "amqp:resource-limit-exceeded"
	NotAllowed            = "amqp:not-allowed"
	InvalidField          = "amqp:invalid-field"
	NotImplemented        = "amqp:not-implemented"
	ResourceLocked        = "amqp:resource-locked"
	PreconditionFailed    = "amqp:precondition-failed"
	ResourceDeleted       = "amqp:resource-deleted"
	IllegalState          = "amqp:illegal-state"
	FrameSizeTooSmall     = "amqp:frame-size-too-small"
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
