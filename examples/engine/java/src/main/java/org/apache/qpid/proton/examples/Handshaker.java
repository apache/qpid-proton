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
package org.apache.qpid.proton.examples;

import org.apache.qpid.proton.engine.BaseHandler;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.Event;
import org.apache.qpid.proton.engine.Link;
import org.apache.qpid.proton.engine.Session;

/**
 * Handshaker
 *
 */

public class Handshaker extends BaseHandler
{

    @Override
    public void onConnectionRemoteOpen(Event evt) {
        Connection conn = evt.getConnection();
        if (conn.getLocalState() == EndpointState.UNINITIALIZED) {
            conn.open();
        }
    }

    @Override
    public void onSessionRemoteOpen(Event evt) {
        Session ssn = evt.getSession();
        if (ssn.getLocalState() == EndpointState.UNINITIALIZED) {
            ssn.open();
        }
    }

    @Override
    public void onLinkRemoteOpen(Event evt) {
        Link link = evt.getLink();
        if (link.getLocalState() == EndpointState.UNINITIALIZED) {
            link.setSource(link.getRemoteSource());
            link.setTarget(link.getRemoteTarget());
            link.open();
        }
    }

    @Override
    public void onConnectionRemoteClose(Event evt) {
        Connection conn = evt.getConnection();
        if (conn.getLocalState() != EndpointState.CLOSED) {
            conn.close();
        }
    }

    @Override
    public void onSessionRemoteClose(Event evt) {
        Session ssn = evt.getSession();
        if (ssn.getLocalState() != EndpointState.CLOSED) {
            ssn.close();
        }
    }

    @Override
    public void onLinkRemoteClose(Event evt) {
        Link link = evt.getLink();
        if (link.getLocalState() != EndpointState.CLOSED) {
            link.close();
        }
    }

}
