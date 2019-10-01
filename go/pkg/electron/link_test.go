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

// Test that link settings are propagated correctly
package electron

import (
	"net"
	"testing"
	"time"

	"github.com/apache/qpid-proton/go/pkg/amqp"
	"github.com/apache/qpid-proton/go/pkg/internal/test"
	"github.com/apache/qpid-proton/go/pkg/proton"
)

func TestLinkSettings(t *testing.T) {
	cConn, sConn := net.Pipe()
	done := make(chan error)
	settings := TerminusSettings{Durability: 1, Expiry: 2, Timeout: 42 * time.Second, Dynamic: true}
	filterMap := map[amqp.Symbol]interface{}{"int": int32(33), "str": "hello"}
	go func() { // Server
		close(done)
		defer sConn.Close()
		c, err := NewConnection(sConn, Server())
		test.FatalIf(t, err)
		for in := range c.Incoming() {
			ep := in.Accept()
			switch ep := ep.(type) {
			case Receiver:
				test.ErrorIf(t, test.Differ("one.source", ep.Source()))
				test.ErrorIf(t, test.Differ(TerminusSettings{}, ep.SourceSettings()))
				test.ErrorIf(t, test.Differ("one.target", ep.Target()))
				test.ErrorIf(t, test.Differ(settings, ep.TargetSettings()))
			case Sender:
				test.ErrorIf(t, test.Differ("two", ep.LinkName()))
				test.ErrorIf(t, test.Differ("two.source", ep.Source()))
				test.ErrorIf(t, test.Differ(TerminusSettings{Durability: proton.Deliveries, Expiry: proton.ExpireNever}, ep.SourceSettings()))
				test.ErrorIf(t, test.Differ(filterMap, ep.Filter()))
			}
		}
	}()

	// Client
	c, err := NewConnection(cConn)
	test.FatalIf(t, err)
	c.Sender(Source("one.source"), Target("one.target"), TargetSettings(settings))

	c.Receiver(Source("two.source"), DurableSubscription("two"), Filter(filterMap))
	c.Close(nil)
	<-done
}
