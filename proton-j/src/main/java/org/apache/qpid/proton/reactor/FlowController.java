/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */

package org.apache.qpid.proton.reactor;

import org.apache.qpid.proton.engine.BaseHandler;
import org.apache.qpid.proton.engine.Event;
import org.apache.qpid.proton.engine.Link;
import org.apache.qpid.proton.engine.Receiver;

/**
 * A handler that applies flow control to a connection.  This handler tops-up
 * link credit each time credit is expended by the receipt of messages.
 */
public class FlowController extends BaseHandler {

    private int drained;
    private int window;

    public FlowController(int window) {
        // XXX: a window of 1 doesn't work because we won't necessarily get
        // notified when the one allowed delivery is settled
        if (window <= 1) throw new IllegalArgumentException();
        this.window = window;
        this.drained = 0;
    }

    public FlowController() {
        this(1024);
    }

    private void topup(Receiver link, int window) {
        int delta = window - link.getCredit();
        link.flow(delta);
    }

    @Override
    public void onUnhandled(Event event) {
        int window = this.window;
        Link link = event.getLink();

        switch(event.getType()) {
        case LINK_LOCAL_OPEN:
        case LINK_REMOTE_OPEN:
        case LINK_FLOW:
        case DELIVERY:
            if (link instanceof Receiver) {
                this.drained += link.drained();
                if (this.drained == 0) {
                    topup((Receiver)link, window);
                }
            }
            break;
        default:
            break;
        }
    }
}
