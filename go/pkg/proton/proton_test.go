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

import (
	"fmt"
	"net"
	"testing"
	"time"

	"github.com/apache/qpid-proton/go/pkg/internal/test"
)

type events []EventType

type testEngine struct {
	Engine
	events chan EventType
}

func newTestEngine(conn net.Conn) (*testEngine, error) {
	testEng := &testEngine{events: make(chan EventType, 1000)}
	return testEng, testEng.Initialize(conn, testEng)
}

func (eng *testEngine) HandleEvent(e Event) {
	eng.events <- e.Type()
}

func (eng *testEngine) expect(events []EventType) error {
	timer := time.After(5 * time.Second)
	for _, want := range events {
		select {
		case got := <-eng.events:
			if want != got {
				return fmt.Errorf("want %s, got %s", want, got)
			}
		case <-timer:
			return fmt.Errorf("expect timeout")
		}
	}
	return nil
}

func Test(t *testing.T) {
	cConn, sConn := net.Pipe()
	client, err := newTestEngine(cConn)
	test.FatalIf(t, err)
	server, err := newTestEngine(sConn)
	test.FatalIf(t, err)
	server.Server()
	go client.Run()
	go server.Run()
	test.FatalIf(t, server.expect(events{EConnectionInit, EConnectionBound}))
	test.FatalIf(t, client.expect(events{EConnectionInit, EConnectionBound}))
	test.FatalIf(t, client.InjectWait(func() error { client.Connection().Open(); return nil }))
	test.FatalIf(t, client.expect(events{EConnectionLocalOpen}))
	test.FatalIf(t, server.expect(events{EConnectionRemoteOpen}))
}
