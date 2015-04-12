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
#include "proton/cpp/Container.h"
#include "proton/cpp/MessagingEvent.h"
#include "proton/cpp/Connection.h"
#include "proton/cpp/Session.h"
#include "proton/cpp/MessagingAdapter.h"

#include "Connector.h"
#include "contexts.h"
#include "proton/connection.h"
#include "proton/session.h"

namespace proton {
namespace cpp {
namespace reactor {

class CHandler : public Handler
{
  public:
    CHandler(pn_handler_t *h) : pnHandler(h) {
        pn_incref(pnHandler);
    }
    ~CHandler() {
        pn_decref(pnHandler);
    }
    pn_handler_t *getPnHandler() { return pnHandler; }
  private:
    pn_handler_t *pnHandler;
};


void dispatch(Handler &h, MessagingEvent &e) {
    // TODO: also dispatch to add()'ed Handlers
    CHandler *chandler;
    int type = e.getType();
    if (type &&  (chandler = dynamic_cast<CHandler*>(&h))) {
        // event and handler are both native Proton C 
        pn_handler_dispatch(chandler->getPnHandler(), e.getPnEvent(), (pn_event_type_t) type);
    }
    else
        e.dispatch(h);
}

// Used to sniff for Connector events before the reactor's global handler sees them.
class OverrideHandler : public Handler
{
  public:
    pn_handler_t *baseHandler;

    OverrideHandler(pn_handler_t *h) : baseHandler(h) {
        pn_incref(baseHandler);
    }
    ~OverrideHandler() {
        pn_decref(baseHandler);
    }

    
    virtual void onUnhandled(Event &e) {
        ProtonEvent *pne = static_cast<ProtonEvent *>(&e);
        // If not a Proton reactor event, nothing to override, nothing to pass along.
        if (!pne) return;
        int type = pne->getType();
        if (!type) return;  // Also not from the reactor

        pn_event_t *cevent = pne->getPnEvent();

        pn_connection_t *conn = pn_event_connection(cevent);
        if (type != PN_CONNECTION_INIT && conn) {
            Connection *connection = getConnectionContext(conn);
            if (connection) {
                Handler *override = connection->getOverride();
                if (override)
                    e.dispatch(*override);
            }
        }

        pn_handler_dispatch(baseHandler, cevent, (pn_event_type_t) type);
    }
};

namespace {

// TODO: configurable policy.  SessionPerConnection for now.
Session getDefaultSession(pn_connection_t *conn, pn_session_t **ses) {
    if (!*ses) {
        *ses = pn_session(conn);
        pn_session_open(*ses);
    }
    return Session(*ses);
}


struct InboundContext {
    Container *container;
    Handler *cppHandler;
};

Container &getContainer(pn_handler_t *c_handler) {
    struct InboundContext *ctxt = (struct InboundContext *) pn_handler_mem(c_handler);
    return *ctxt->container;
}

Handler &getCppHandler(pn_handler_t *c_handler) {
    struct InboundContext *ctxt = (struct InboundContext *) pn_handler_mem(c_handler);
    return *ctxt->cppHandler;
}

void cpp_handler_dispatch(pn_handler_t *c_handler, pn_event_t *cevent, pn_event_type_t type)
{
    MessagingEvent ev(cevent, type, getContainer(c_handler));
    dispatch(getCppHandler(c_handler), ev);
}

pn_handler_t *cpp_handler(Container *c, Handler *h)
{
    pn_handler_t *handler = pn_handler_new(cpp_handler_dispatch, sizeof(struct InboundContext), 0);
    struct InboundContext *ctxt = (struct InboundContext *) pn_handler_mem(handler);
    ctxt->container = c;
    ctxt->cppHandler = h;
    return handler;
}


} // namespace


Container::Container(MessagingHandler &mhandler) :
    reactor(0), messagingHandler(mhandler) {
}

Connection &Container::connect(std::string &host) {
    if (!reactor) throw "Container not initialized";
    Connection *connection = new Connection(*this);
    Connector *connector = new Connector(*connection);
    connector->setAddress(host);  // TODO: url vector
    connection->setOverride(connector);
    connection->open();
    return *connection;
}

pn_reactor_t *Container::getReactor() { return reactor; }

pn_handler_t *Container::getGlobalHandler() { return globalHandler; }

Sender Container::createSender(Connection &connection, std::string &addr) { 
    Session session = getDefaultSession(connection.pnConnection, &connection.defaultSession);
    Sender snd = session.createSender(addr);
    pn_terminus_set_address(pn_link_target(snd.getPnLink()), addr.c_str());
    snd.open();
    return snd;
}

Receiver Container::createReceiver(Connection &connection, std::string &addr) { 
    Session session = getDefaultSession(connection.pnConnection, &connection.defaultSession);
    Receiver rcv = session.createReceiver(addr);
    pn_terminus_set_address(pn_link_source(rcv.getPnLink()), addr.c_str());
    rcv.open();
    return rcv;
}


void Container::run() {
    reactor = pn_reactor();
    // Set our context on the reactor
    setContainerContext(reactor, this);

    // Set the reactor's main/default handler (see note below)
    MessagingAdapter messagingAdapter(messagingHandler);
    messagingHandler.addChildHandler(messagingAdapter);
    pn_handler_t *cppHandler = cpp_handler(this, &messagingHandler);
    pn_reactor_set_handler(reactor, cppHandler);

    // Set our own global handler that "subclasses" the existing one
    pn_handler_t *cGlobalHandler = pn_reactor_get_global_handler(reactor);
    pn_incref(cGlobalHandler);
    OverrideHandler overrideHandler(cGlobalHandler);
    pn_handler_t *cppGlobalHandler = cpp_handler(this, &overrideHandler);
    pn_reactor_set_global_handler(reactor, cppGlobalHandler);

    // Note: we have just set up the following 4 handlers that see events in this order:
    // messagingHandler, messagingAdapter, connector override, the reactor's default global
    // handler (pn_iohandler)
    // TODO: remove fifth pn_handshaker once messagingAdapter matures

    pn_reactor_run(reactor);
    pn_decref(cGlobalHandler);
    pn_reactor_free(reactor);
}

}}} // namespace proton::cpp::reactor
