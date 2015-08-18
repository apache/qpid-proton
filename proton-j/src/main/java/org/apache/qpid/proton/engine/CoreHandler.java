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
package org.apache.qpid.proton.engine;

public interface CoreHandler extends Handler {
    void onConnectionInit(Event e);
    void onConnectionLocalOpen(Event e);
    void onConnectionRemoteOpen(Event e);
    void onConnectionLocalClose(Event e);
    void onConnectionRemoteClose(Event e);
    void onConnectionBound(Event e);
    void onConnectionUnbound(Event e);
    void onConnectionFinal(Event e);

    void onSessionInit(Event e);
    void onSessionLocalOpen(Event e);
    void onSessionRemoteOpen(Event e);
    void onSessionLocalClose(Event e);
    void onSessionRemoteClose(Event e);
    void onSessionFinal(Event e);

    void onLinkInit(Event e);
    void onLinkLocalOpen(Event e);
    void onLinkRemoteOpen(Event e);
    void onLinkLocalDetach(Event e);
    void onLinkRemoteDetach(Event e);
    void onLinkLocalClose(Event e);
    void onLinkRemoteClose(Event e);
    void onLinkFlow(Event e);
    void onLinkFinal(Event e);

    void onDelivery(Event e);
    void onTransport(Event e);
    void onTransportError(Event e);
    void onTransportHeadClosed(Event e);
    void onTransportTailClosed(Event e);
    void onTransportClosed(Event e);

    void onReactorInit(Event e);
    void onReactorQuiesced(Event e);
    void onReactorFinal(Event e);

    void onTimerTask(Event e);

    void onSelectableInit(Event e);
    void onSelectableUpdated(Event e);
    void onSelectableReadable(Event e);
    void onSelectableWritable(Event e);
    void onSelectableExpired(Event e);
    void onSelectableError(Event e);
    void onSelectableFinal(Event e);

}
