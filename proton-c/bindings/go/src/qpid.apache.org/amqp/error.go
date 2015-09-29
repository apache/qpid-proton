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
)

// Error is an AMQP error condition. It has a name and a description.
// It implements the Go error interface so can be returned as an error value.
//
// You can pass amqp.Error to methods that pass an error to a remote endpoint,
// this gives you full control over what the remote endpoint will see.
//
// You can also pass any Go error to such functions, the remote peer
// will see the equivalent of MakeError(error)
//
type Error struct{ Name, Description string }

// Error implements the Go error interface for AMQP error errors.
func (c Error) Error() string { return fmt.Sprintf("proton %s: %s", c.Name, c.Description) }

// Errorf makes a Error with name and formatted description as per fmt.Sprintf
func Errorf(name, format string, arg ...interface{}) Error {
	return Error{name, fmt.Sprintf(format, arg...)}
}

// MakeError makes an AMQP error from a go error using the Go error type as the name
// and the err.Error() string as the description.
func MakeError(err error) Error {
	return Error{reflect.TypeOf(err).Name(), err.Error()}
}

var (
	InternalError      = "amqp:internal-error"
	NotFound           = "amqp:not-found"
	UnauthorizedAccess = "amqp:unauthorized-access"
	DecodeError        = "amqp:decode-error"
	ResourceLimit      = "amqp:resource-limit"
	NotAllowed         = "amqp:not-allowed"
	InvalidField       = "amqp:invalid-field"
	NotImplemented     = "amqp:not-implemented"
	ResourceLocked     = "amqp:resource-locked"
	PreerrorFailed     = "amqp:preerror-failed"
	ResourceDeleted    = "amqp:resource-deleted"
	IllegalState       = "amqp:illegal-state"
	FrameSizeTooSmall  = "amqp:frame-size-too-small"
)
