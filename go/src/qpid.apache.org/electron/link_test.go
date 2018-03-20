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
	"qpid.apache.org/amqp"
	"qpid.apache.org/proton"
	"testing"
	"time"
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
		fatalIf(t, err)
		for in := range c.Incoming() {
			ep := in.Accept()
			switch ep := ep.(type) {
			case Receiver:
				errorIf(t, checkEqual("one.source", ep.Source()))
				errorIf(t, checkEqual(TerminusSettings{}, ep.SourceSettings()))
				errorIf(t, checkEqual("one.target", ep.Target()))
				errorIf(t, checkEqual(settings, ep.TargetSettings()))
			case Sender:
				errorIf(t, checkEqual("two", ep.LinkName()))
				errorIf(t, checkEqual("two.source", ep.Source()))
				errorIf(t, checkEqual(TerminusSettings{Durability: proton.Deliveries, Expiry: proton.ExpireNever}, ep.SourceSettings()))
				errorIf(t, checkEqual(filterMap, ep.Filter()))
			}
		}
	}()

	// Client
	c, err := NewConnection(cConn)
	fatalIf(t, err)
	c.Sender(Source("one.source"), Target("one.target"), TargetSettings(settings))

	c.Receiver(Source("two.source"), DurableSubscription("two"), Filter(filterMap))
	c.Close(nil)
	<-done
}
