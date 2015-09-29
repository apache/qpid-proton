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

package internal

import (
	"fmt"
	"math/rand"
	"strconv"
	"sync"
	"sync/atomic"
	"time"
)

type UUID [16]byte

func (u UUID) String() string {
	return fmt.Sprintf("%X-%X-%X-%X-%X", u[0:4], u[4:6], u[6:8], u[8:10], u[10:])
}

// Don't mess with the default random source.
var randomSource = rand.NewSource(time.Now().UnixNano())
var randomLock sync.Mutex

func random() byte {
	randomLock.Lock()
	defer randomLock.Unlock()
	return byte(randomSource.Int63())
}

func UUID4() UUID {
	var u UUID
	for i := 0; i < len(u); i++ {
		u[i] = random()
	}
	// See /https://tools.ietf.org/html/rfc4122#section-4.4
	u[6] = (u[6] & 0x0F) | 0x40 // Version bits to 4
	u[8] = (u[8] & 0x3F) | 0x80 // Reserved bits (top two) set to 01
	return u
}

// A simple atomic counter to generate unique 64 bit IDs.
type IdCounter struct{ count uint64 }

// NextInt gets the next uint64 value from the atomic counter.
func (uc *IdCounter) NextInt() uint64 {
	return atomic.AddUint64(&uc.count, 1)
}

// Next gets the next integer value encoded as a base32 string, safe for NUL
// terminated C strings.
func (uc *IdCounter) Next() string {
	return strconv.FormatUint(uc.NextInt(), 32)
}
