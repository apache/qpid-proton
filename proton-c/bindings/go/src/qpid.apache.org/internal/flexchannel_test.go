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
	"testing"
)

func recvall(ch <-chan interface{}) (result []interface{}) {
	for {
		select {
		case x := <-ch:
			result = append(result, x)
		default:
			return
		}
	}
}

func sendall(data []interface{}, ch chan<- interface{}) {
}

func TestFlex(t *testing.T) {
	fc := NewFlexChannel(5)

	// Test send/receve
	go func() {
		for i := 0; i < 4; i++ {
			fc.In <- i
		}
	}()

	for i := 0; i < 4; i++ {
		j := <-fc.Out
		if i != j {
			t.Error("%v != %v", i, j)
		}
	}
	select {
	case x, ok := <-fc.Out:
		t.Error("receive empty channel got", x, ok)
	default:
	}

	// Test buffer limit
	for i := 10; i < 15; i++ {
		fc.In <- i
	}
	select {
	case fc.In <- 0:
		t.Error("send to full channel did not block")
	default:
	}
	i := <-fc.Out
	if i != 10 {
		t.Error("%v != %v", i, 10)
	}
	fc.In <- 15
	close(fc.In)

	for i := 11; i < 16; i++ {
		j := <-fc.Out
		if i != j {
			t.Error("%v != %v", i, j)
		}
	}

	x, ok := <-fc.Out
	if ok {
		t.Error("Unexpected value on Out", x)
	}
}
