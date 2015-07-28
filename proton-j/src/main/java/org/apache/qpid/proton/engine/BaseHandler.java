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

import java.util.Iterator;
import java.util.LinkedHashSet;


/**
 * BaseHandler
 *
 */

public class BaseHandler implements CoreHandler
{

    public static Handler getHandler(Record r) {
        return r.get(Handler.class, Handler.class);
    }

    public static void setHandler(Record r, Handler handler) {
        r.set(Handler.class, Handler.class, handler);
    }

    public static Handler getHandler(Extendable ext) {
        return ext.attachments().get(Handler.class, Handler.class);
    }

    public static void setHandler(Extendable ext, Handler handler) {
        ext.attachments().set(Handler.class, Handler.class, handler);
    }

    private LinkedHashSet<Handler> children = new LinkedHashSet<Handler>();

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

	@Override
	public void handle(Event e) {
        switch (e.getType()) {
        case CONNECTION_INIT:
            onConnectionInit(e);
            break;
        case CONNECTION_LOCAL_OPEN:
            onConnectionLocalOpen(e);
            break;
        case CONNECTION_REMOTE_OPEN:
            onConnectionRemoteOpen(e);
            break;
        case CONNECTION_LOCAL_CLOSE:
            onConnectionLocalClose(e);
            break;
        case CONNECTION_REMOTE_CLOSE:
            onConnectionRemoteClose(e);
            break;
        case CONNECTION_BOUND:
            onConnectionBound(e);
            break;
        case CONNECTION_UNBOUND:
            onConnectionUnbound(e);
            break;
        case CONNECTION_FINAL:
            onConnectionFinal(e);
            break;
        case SESSION_INIT:
            onSessionInit(e);
            break;
        case SESSION_LOCAL_OPEN:
            onSessionLocalOpen(e);
            break;
        case SESSION_REMOTE_OPEN:
            onSessionRemoteOpen(e);
            break;
        case SESSION_LOCAL_CLOSE:
            onSessionLocalClose(e);
            break;
        case SESSION_REMOTE_CLOSE:
            onSessionRemoteClose(e);
            break;
        case SESSION_FINAL:
            onSessionFinal(e);
            break;
        case LINK_INIT:
            onLinkInit(e);
            break;
        case LINK_LOCAL_OPEN:
            onLinkLocalOpen(e);
            break;
        case LINK_REMOTE_OPEN:
            onLinkRemoteOpen(e);
            break;
        case LINK_LOCAL_DETACH:
            onLinkLocalDetach(e);
            break;
        case LINK_REMOTE_DETACH:
            onLinkRemoteDetach(e);
            break;
        case LINK_LOCAL_CLOSE:
            onLinkLocalClose(e);
            break;
        case LINK_REMOTE_CLOSE:
            onLinkRemoteClose(e);
            break;
        case LINK_FLOW:
            onLinkFlow(e);
            break;
        case LINK_FINAL:
            onLinkFinal(e);
            break;
        case DELIVERY:
            onDelivery(e);
            break;
        case TRANSPORT:
            onTransport(e);
            break;
        case TRANSPORT_ERROR:
            onTransportError(e);
            break;
        case TRANSPORT_HEAD_CLOSED:
            onTransportHeadClosed(e);
            break;
        case TRANSPORT_TAIL_CLOSED:
            onTransportTailClosed(e);
            break;
        case TRANSPORT_CLOSED:
            onTransportClosed(e);
            break;
        case REACTOR_FINAL:
            onReactorFinal(e);
            break;
        case REACTOR_QUIESCED:
            onReactorQuiesced(e);
            break;
        case REACTOR_INIT:
            onReactorInit(e);
            break;
        case SELECTABLE_ERROR:
            onSelectableError(e);
            break;
        case SELECTABLE_EXPIRED:
            onSelectableExpired(e);
            break;
        case SELECTABLE_FINAL:
            onSelectableFinal(e);
            break;
        case SELECTABLE_INIT:
            onSelectableInit(e);
            break;
        case SELECTABLE_READABLE:
            onSelectableReadable(e);
            break;
        case SELECTABLE_UPDATED:
            onSelectableWritable(e);
            break;
        case SELECTABLE_WRITABLE:
            onSelectableWritable(e);
            break;
        case TIMER_TASK:
            onTimerTask(e);
            break;
        case NON_CORE_EVENT:
            onUnhandled(e);
            break;
        }
	}
}
