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

import java.util.HashSet;
import java.util.Iterator;


/**
 * BaseHandler
 *
 */

public class BaseHandler implements Handler
{

    public static Handler getHandler(Extendable ext) {
        return ext.attachments().get(Handler.class, Handler.class);
    }

    public static void setHandler(Extendable ext, Handler handler) {
        ext.attachments().set(Handler.class, Handler.class, handler);
    }

    private HashSet<Handler> children = new HashSet<Handler>();

    @Override public void onConnectionInit(Event e) { onUnhandled(e); }
    @Override public void onConnectionLocalOpen(Event e) { onUnhandled(e); }
    @Override public void onConnectionRemoteOpen(Event e) { onUnhandled(e); }
    @Override public void onConnectionLocalClose(Event e) { onUnhandled(e); }
    @Override public void onConnectionRemoteClose(Event e) { onUnhandled(e); }
    @Override public void onConnectionBound(Event e) { onUnhandled(e); }
    @Override public void onConnectionUnbound(Event e) { onUnhandled(e); }
    @Override public void onConnectionFinal(Event e) { onUnhandled(e); }

    @Override public void onSessionInit(Event e) { onUnhandled(e); }
    @Override public void onSessionLocalOpen(Event e) { onUnhandled(e); }
    @Override public void onSessionRemoteOpen(Event e) { onUnhandled(e); }
    @Override public void onSessionLocalClose(Event e) { onUnhandled(e); }
    @Override public void onSessionRemoteClose(Event e) { onUnhandled(e); }
    @Override public void onSessionFinal(Event e) { onUnhandled(e); }

    @Override public void onLinkInit(Event e) { onUnhandled(e); }
    @Override public void onLinkLocalOpen(Event e) { onUnhandled(e); }
    @Override public void onLinkRemoteOpen(Event e) { onUnhandled(e); }
    @Override public void onLinkLocalDetach(Event e) { onUnhandled(e); }
    @Override public void onLinkRemoteDetach(Event e) { onUnhandled(e); }
    @Override public void onLinkLocalClose(Event e) { onUnhandled(e); }
    @Override public void onLinkRemoteClose(Event e) { onUnhandled(e); }
    @Override public void onLinkFlow(Event e) { onUnhandled(e); }
    @Override public void onLinkFinal(Event e) { onUnhandled(e); }

    @Override public void onDelivery(Event e) { onUnhandled(e); }
    @Override public void onTransport(Event e) { onUnhandled(e); }
    @Override public void onTransportError(Event e) { onUnhandled(e); }
    @Override public void onTransportHeadClosed(Event e) { onUnhandled(e); }
    @Override public void onTransportTailClosed(Event e) { onUnhandled(e); }
    @Override public void onTransportClosed(Event e) { onUnhandled(e); }

    @Override public void onReactorInit(Event e) { onUnhandled(e); }
    @Override public void onReactorQuiesced(Event e) { onUnhandled(e); }
    @Override public void onReactorFinal(Event e) { onUnhandled(e); }

    @Override public void onTimerTask(Event e) { onUnhandled(e); }

    @Override public void onSelectableInit(Event e) { onUnhandled(e); }
    @Override public void onSelectableUpdated(Event e) { onUnhandled(e); }
    @Override public void onSelectableReadable(Event e) { onUnhandled(e); }
    @Override public void onSelectableWritable(Event e) { onUnhandled(e); }
    @Override public void onSelectableExpired(Event e) { onUnhandled(e); }
    @Override public void onSelectableError(Event e) { onUnhandled(e); }
    @Override public void onSelectableFinal(Event e) { onUnhandled(e); }

    @Override public void onUnhandled(Event event) {}

    @Override
    public void add(Handler child) {
        children.add(child);
    }

    @Override
    public Iterator<Handler> children() {
        return children.iterator();
    }

}
