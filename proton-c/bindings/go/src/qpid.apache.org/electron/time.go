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

package electron

import (
	"fmt"
	"math"
	"reflect"
	"time"
)

// Timeout is the error returned if an operation does not complete on time.
//
// Methods named *Timeout in this package take time.Duration timeout parameter.
//
// If timeout > 0 and there is no result available before the timeout, they
// return a zero or nil value and Timeout as an error.
//
// If timeout == 0 they will return a result if one is immediately available or
// nil/zero and Timeout as an error if not.
//
// If timeout == Forever the function will return only when there is a result or
// some non-timeout error occurs.
//
var Timeout = fmt.Errorf("timeout")

// Forever can be used as a timeout parameter to indicate wait forever.
const Forever time.Duration = math.MaxInt64

// timedReceive receives on channel (which can be a chan of any type), waiting
// up to timeout.
//
// timeout==0 means do a non-blocking receive attempt. timeout < 0 means block
// forever. Other values mean block up to the timeout.
//
// Returns error Timeout on timeout, Closed on channel close.
func timedReceive(channel interface{}, timeout time.Duration) (interface{}, error) {
	cases := []reflect.SelectCase{
		reflect.SelectCase{Dir: reflect.SelectRecv, Chan: reflect.ValueOf(channel)},
	}
	if timeout == 0 { // Non-blocking
		cases = append(cases, reflect.SelectCase{Dir: reflect.SelectDefault})
	} else { // Block up to timeout
		cases = append(cases,
			reflect.SelectCase{Dir: reflect.SelectRecv, Chan: reflect.ValueOf(After(timeout))})
	}
	chosen, value, ok := reflect.Select(cases)
	switch {
	case chosen == 0 && ok:
		return value.Interface(), nil
	case chosen == 0 && !ok:
		return nil, Closed
	default:
		return nil, Timeout
	}
}

// After is like time.After but returns a nil channel if timeout == Forever
// since selecting on a nil channel will never return.
func After(timeout time.Duration) <-chan time.Time {
	if timeout == Forever {
		return nil
	} else {
		return time.After(timeout)
	}
}
