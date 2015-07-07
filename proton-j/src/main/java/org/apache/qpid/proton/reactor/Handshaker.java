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
import org.apache.qpid.proton.engine.Endpoint;
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.Event;
import org.apache.qpid.proton.engine.Link;

/**
 * A handler that mirrors the actions of the remote end of a connection.  This
 * handler responds in kind when the remote end of the connection is opened and
 * closed.  Likewise if the remote end of the connection opens or closes
 * sessions and links, this handler responds by opening or closing the local end
 * of the session or link.
 */
public class Handshaker extends BaseHandler {

    private void open(Endpoint endpoint) {
        if (endpoint.getLocalState() == EndpointState.UNINITIALIZED) {
            endpoint.open();
        }
    }

    private void close(Endpoint endpoint) {
        if (endpoint.getLocalState() != EndpointState.CLOSED) {
            endpoint.close();
        }
    }

    @Override
    public void onConnectionRemoteOpen(Event event) {
        open(event.getConnection());
    }

    @Override
    public void onSessionRemoteOpen(Event event) {
        open(event.getSession());
    }

    @Override
    public void onLinkRemoteOpen(Event event) {
        Link link = event.getLink();
        if (link.getLocalState() == EndpointState.UNINITIALIZED) {
            if (link.getRemoteSource() != null) {
                link.setSource(link.getRemoteSource().copy());
            }
            if (link.getRemoteTarget() != null) {
                link.setTarget(link.getRemoteTarget().copy());
            }
        }
        open(link);
    }

    @Override
    public void onConnectionRemoteClose(Event event) {
        close(event.getConnection());
    }

    @Override
    public void onSessionRemoteClose(Event event) {
        close(event.getSession());
    }

    @Override
    public void onLinkRemoteClose(Event event) {
        close(event.getLink());
    }
}
