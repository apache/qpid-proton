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
	"flag"
	"strings"
	"sync"
	"testing"

	"github.com/apache/qpid-proton/go/pkg/amqp"
	"github.com/apache/qpid-proton/go/pkg/internal/test"
)

// To change capacity use
//     go test -bench=. -args -capacity 100
var capacity = flag.Int("capacity", 1000, "Prefetch capacity")
var bodySize = flag.Int("bodySize", 1000, "Message body size")

type bmCommon struct {
	b    *testing.B
	p    *pair
	s    Sender
	r    Receiver
	ack  chan Outcome
	done sync.WaitGroup
}

func newBmCommon(p *pair, waitCount int) *bmCommon {
	bm := bmCommon{p: p, b: p.t.(*testing.B)}
	bm.p.capacity = *capacity
	bm.p.prefetch = true
	bm.s, bm.r = p.sender()
	bm.ack = make(chan Outcome, *capacity)
	bm.done.Add(waitCount)
	bm.b.ResetTimer()
	return &bm
}

func (bm *bmCommon) receiveAccept() {
	defer bm.done.Done()
	for n := 0; n < bm.b.N; n++ {
		if rm, err := bm.r.Receive(); err != nil {
			bm.b.Fatal(err)
		} else {
			test.FatalIf(bm.b, rm.Accept())
		}
	}
}

func (bm *bmCommon) outcomes() {
	defer bm.done.Done()
	for n := 0; n < bm.b.N; n++ {
		test.FatalIf(bm.b, (<-bm.ack).Error)
	}
}

var emptyMsg = amqp.NewMessage()

func BenchmarkSendForget(b *testing.B) {
	bm := newBmCommon(newPipe(b, nil, nil), 1)
	defer bm.p.close()

	go func() { // Receive, no ack
		defer bm.done.Done()
		for n := 0; n < b.N; n++ {
			if _, err := bm.r.Receive(); err != nil {
				b.Fatal(err)
			}
		}
	}()

	for n := 0; n < b.N; n++ {
		bm.s.SendForget(emptyMsg)
	}
	bm.done.Wait()
}

func BenchmarkSendSync(b *testing.B) {
	bm := newBmCommon(newPipe(b, nil, nil), 1)
	defer bm.p.close()

	go bm.receiveAccept()
	for n := 0; n < b.N; n++ {
		test.FatalIf(b, bm.s.SendSync(emptyMsg).Error)
	}
	bm.done.Wait()
}

func BenchmarkSendAsync(b *testing.B) {
	bm := newBmCommon(newPipe(b, nil, nil), 2)
	defer bm.p.close()

	go bm.outcomes()      // Handle outcomes
	go bm.receiveAccept() // Receive
	for n := 0; n < b.N; n++ {
		bm.s.SendAsync(emptyMsg, bm.ack, nil)
	}
	bm.done.Wait()
}

// Create a new message for each send, with body and property.
func BenchmarkSendAsyncNewMessage(b *testing.B) {
	body := strings.Repeat("x", *bodySize)
	bm := newBmCommon(newPipe(b, nil, nil), 2)
	defer bm.p.close()

	go bm.outcomes()      // Handle outcomes
	go bm.receiveAccept() // Receive
	for n := 0; n < b.N; n++ {
		msg := amqp.NewMessageWith(body)
		msg.SetApplicationProperties(map[string]interface{}{"prop": "value"})
		bm.s.SendAsync(msg, bm.ack, nil)
	}
	bm.done.Wait()
}
