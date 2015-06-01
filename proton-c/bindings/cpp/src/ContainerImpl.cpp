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
#include "proton/cpp/Acceptor.h"
#include "proton/cpp/exceptions.h"

#include "Msg.h"
#include "ContainerImpl.h"
#include "ConnectionImpl.h"
#include "Connector.h"
#include "contexts.h"
#include "Url.h"
#include "platform.h"
#include "PrivateImplRef.h"

#include "proton/connection.h"
#include "proton/session.h"
#include "proton/handlers.h"

namespace proton {
namespace reactor {

namespace {

ConnectionImpl *getImpl(const Connection &c) {
    return PrivateImplRef<Connection>::get(c);
}

ContainerImpl *getImpl(const Container &c) {
    return PrivateImplRef<Container>::get(c);
}

} // namespace


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

    virtual void onUnhandled(Event &e) {
        ProtonEvent *pne = dynamic_cast<ProtonEvent *>(&e);
        if (!pne) return;
        int type = pne->getType();
        if (!type) return;  // Not from the reactor
        pn_handler_dispatch(pnHandler, pne->getPnEvent(), (pn_event_type_t) type);
    }

  private:
    pn_handler_t *pnHandler;
};


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
        ProtonEvent *pne = dynamic_cast<ProtonEvent *>(&e);
        // If not a Proton reactor event, nothing to override, nothing to pass along.
        if (!pne) return;
        int type = pne->getType();
        if (!type) return;  // Also not from the reactor

        pn_event_t *cevent = pne->getPnEvent();
        pn_connection_t *conn = pn_event_connection(cevent);
        if (conn && type != PN_CONNECTION_INIT) {
            // send to override handler first
            ConnectionImpl *connection = getConnectionContext(conn);
            if (connection) {
                Handler *override = connection->getOverride();
                if (override) {
                    e.dispatch(*override);
                }
            }
        }

        pn_handler_dispatch(baseHandler, cevent, (pn_event_type_t) type);

        if (conn && type == PN_CONNECTION_FINAL) {
            //  TODO:  this must be the last action of the last handler looking at
            //  connection events. Better: generate a custom FINAL event (or task).  Or move to
            //  separate event streams per connection as part of multi threading support.
            ConnectionImpl *cimpl = getConnectionContext(conn);
            if (cimpl)
                cimpl->reactorDetach();
            // TODO: remember all connections and do reactorDetach of zombie connections
            // not yet pn_connection_release'd at PN_REACTOR_FINAL.
        }
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
    ContainerImpl *containerImpl;
    Handler *cppHandler;
};

ContainerImpl *getContainerImpl(pn_handler_t *c_handler) {
    struct InboundContext *ctxt = (struct InboundContext *) pn_handler_mem(c_handler);
    return ctxt->containerImpl;
}

Handler &getCppHandler(pn_handler_t *c_handler) {
    struct InboundContext *ctxt = (struct InboundContext *) pn_handler_mem(c_handler);
    return *ctxt->cppHandler;
}

void cpp_handler_dispatch(pn_handler_t *c_handler, pn_event_t *cevent, pn_event_type_t type)
{
    Container c(getContainerImpl(c_handler)); // Ref counted per event, but when is the last event if stop() never called?
    MessagingEvent mevent(cevent, type, c);
    mevent.dispatch(getCppHandler(c_handler));
}

void cpp_handler_cleanup(pn_handler_t *c_handler)
{
}

pn_handler_t *cpp_handler(ContainerImpl *c, Handler *h)
{
    pn_handler_t *handler = pn_handler_new(cpp_handler_dispatch, sizeof(struct InboundContext), cpp_handler_cleanup);
    struct InboundContext *ctxt = (struct InboundContext *) pn_handler_mem(handler);
    ctxt->containerImpl = c;
    ctxt->cppHandler = h;
    return handler;
}


} // namespace


void ContainerImpl::incref(ContainerImpl *impl) {
    impl->refCount++;
}

void ContainerImpl::decref(ContainerImpl *impl) {
    impl->refCount--;
    if (impl->refCount == 0)
        delete impl;
}

ContainerImpl::ContainerImpl(Handler &h) :
    reactor(0), handler(&h), messagingAdapter(0),
    overrideHandler(0), flowController(0), containerId(generateUuid()),
    refCount(0)
{}

ContainerImpl::ContainerImpl() :
    reactor(0), handler(0), messagingAdapter(0),
    overrideHandler(0), flowController(0), containerId(generateUuid()),
    refCount(0)
{}

ContainerImpl::~ContainerImpl() {
    delete overrideHandler;
    delete flowController;
    delete messagingAdapter;
    pn_reactor_free(reactor);
}

Connection ContainerImpl::connect(std::string &host, Handler *h) {
    if (!reactor) throw ProtonException(MSG("Container not started"));
    Container cntnr(this);
    Connection connection(cntnr, handler);
    Connector *connector = new Connector(connection);
    // Connector self-deletes depending on reconnect logic
    connector->setAddress(host);  // TODO: url vector
    connection.setOverride(connector);
    connection.open();
    return connection;
}

pn_reactor_t *ContainerImpl::getReactor() { return reactor; }


std::string ContainerImpl::getContainerId() { return containerId; }

Duration ContainerImpl::getTimeout() {
    pn_millis_t tmo = pn_reactor_get_timeout(reactor);
    if (tmo == PN_MILLIS_MAX)
        return Duration::FOREVER;
    return Duration(tmo);
}

void ContainerImpl::setTimeout(Duration timeout) {
    if (timeout == Duration::FOREVER || timeout.getMilliseconds() > PN_MILLIS_MAX)
        pn_reactor_set_timeout(reactor, PN_MILLIS_MAX);
    else {
        pn_millis_t tmo = timeout.getMilliseconds();
        pn_reactor_set_timeout(reactor, tmo);
    }
}


Sender ContainerImpl::createSender(Connection &connection, std::string &addr, Handler *h) {
    if (!reactor) throw ProtonException(MSG("Container not started"));
    Session session = getDefaultSession(connection.getPnConnection(), &getImpl(connection)->defaultSession);
    Sender snd = session.createSender(containerId  + '-' + addr);
    pn_link_t *lnk = snd.getPnLink();
    pn_terminus_set_address(pn_link_target(lnk), addr.c_str());
    if (h) {
        pn_record_t *record = pn_link_attachments(lnk);
        pn_record_set_handler(record, wrapHandler(h));
    }
    snd.open();

    ConnectionImpl *connImpl = getImpl(connection);
    return snd;
}

Sender ContainerImpl::createSender(std::string &urlString) {
    if (!reactor) throw ProtonException(MSG("Container not started"));
    Connection conn = connect(urlString, 0);
    Session session = getDefaultSession(conn.getPnConnection(), &getImpl(conn)->defaultSession);
    std::string path = Url(urlString).getPath();
    Sender snd = session.createSender(containerId + '-' + path);
    pn_terminus_set_address(pn_link_target(snd.getPnLink()), path.c_str());
    snd.open();

    ConnectionImpl *connImpl = getImpl(conn);
    return snd;
}

Receiver ContainerImpl::createReceiver(Connection &connection, std::string &addr) {
    if (!reactor) throw ProtonException(MSG("Container not started"));
    ConnectionImpl *connImpl = getImpl(connection);
    Session session = getDefaultSession(connImpl->pnConnection, &connImpl->defaultSession);
    Receiver rcv = session.createReceiver(containerId + '-' + addr);
    pn_terminus_set_address(pn_link_source(rcv.getPnLink()), addr.c_str());
    rcv.open();
    return rcv;
}

Receiver ContainerImpl::createReceiver(const std::string &urlString) {
    if (!reactor) throw ProtonException(MSG("Container not started"));
    // TODO: const cleanup of API
    Connection conn = connect(const_cast<std::string &>(urlString), 0);
    Session session = getDefaultSession(conn.getPnConnection(), &getImpl(conn)->defaultSession);
    std::string path = Url(urlString).getPath();
    Receiver rcv = session.createReceiver(containerId + '-' + path);
    pn_terminus_set_address(pn_link_source(rcv.getPnLink()), path.c_str());
    rcv.open();
    return rcv;
}

Acceptor ContainerImpl::acceptor(const std::string &host, const std::string &port) {
    pn_acceptor_t *acptr = pn_reactor_acceptor(reactor, host.c_str(), port.c_str(), NULL);
    if (acptr)
        return Acceptor(acptr);
    else
        throw ProtonException(MSG("accept fail: " << pn_error_text(pn_io_error(pn_reactor_io(reactor))) << "(" << host << ":" << port << ")"));
}

Acceptor ContainerImpl::listen(const std::string &urlString) {
    if (!reactor) throw ProtonException(MSG("Container not started"));
    Url url(urlString);
    // TODO: SSL
    return acceptor(url.getHost(), url.getPort());
}


pn_handler_t *ContainerImpl::wrapHandler(Handler *h) {
    return cpp_handler(this, h);
}


void ContainerImpl::initializeReactor() {
    if (reactor) throw ProtonException(MSG("Container already running"));
    reactor = pn_reactor();

    // Set our context on the reactor
    setContainerContext(reactor, this);

    if (handler) {
        pn_handler_t *cppHandler = cpp_handler(this, handler);
        pn_reactor_set_handler(reactor, cppHandler);
        pn_decref(cppHandler);
    }

    // Set our own global handler that "subclasses" the existing one
    pn_handler_t *globalHandler = pn_reactor_get_global_handler(reactor);
    overrideHandler = new OverrideHandler(globalHandler);
    pn_handler_t *cppGlobalHandler = cpp_handler(this, overrideHandler);
    pn_reactor_set_global_handler(reactor, cppGlobalHandler);
    pn_decref(cppGlobalHandler);

    // Note: we have just set up the following 4/5 handlers that see events in this order:
    // messagingHandler (Proton C events), pn_flowcontroller (optional), messagingAdapter,
    // messagingHandler (Messaging events from the messagingAdapter, i.e. the delegate),
    // connector override, the reactor's default globalhandler (pn_iohandler)
}

void ContainerImpl::run() {
    initializeReactor();
    pn_reactor_run(reactor);
}

void ContainerImpl::start() {
    initializeReactor();
    pn_reactor_start(reactor);
}

bool ContainerImpl::process() {
    if (!reactor) throw ProtonException(MSG("Container not started"));
    bool result = pn_reactor_process(reactor);
    // TODO: check errors
    return result;
}

void ContainerImpl::stop() {
    if (!reactor) throw ProtonException(MSG("Container not started"));
    pn_reactor_stop(reactor);
    // TODO: check errors
}

void ContainerImpl::wakeup() {
    if (!reactor) throw ProtonException(MSG("Container not started"));
    pn_reactor_wakeup(reactor);
    // TODO: check errors
}

bool ContainerImpl::isQuiesced() {
    if (!reactor) throw ProtonException(MSG("Container not started"));
    return pn_reactor_quiesced(reactor);
}

}} // namespace proton::reactor
