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
#include "proton/cpp/MessagingHandler.h"
#include "proton/cpp/ProtonEvent.h"
#include "proton/cpp/MessagingAdapter.h"
#include "proton/handlers.h"

namespace proton {
namespace reactor {

namespace {
class CFlowController : public ProtonHandler
{
  public:
    pn_handler_t *flowcontroller;

    CFlowController(int window) : flowcontroller(pn_flowcontroller(window)) {}
    ~CFlowController() {
        pn_decref(flowcontroller);
    }

    void redirect(Event &e) {
        ProtonEvent *pne = dynamic_cast<ProtonEvent *>(&e);
        pn_handler_dispatch(flowcontroller, pne->getPnEvent(), (pn_event_type_t) pne->getType());
    }

    virtual void onLinkLocalOpen(Event &e) { redirect(e); }
    virtual void onLinkRemoteOpen(Event &e) { redirect(e); }
    virtual void onLinkFlow(Event &e) { redirect(e); }
    virtual void onDelivery(Event &e) { redirect(e); }
};

} // namespace




MessagingHandler::MessagingHandler(int prefetch0, bool autoAccept0, bool autoSettle0, bool peerCloseIsError0) :
    prefetch(prefetch0), autoAccept(autoAccept0), autoSettle(autoSettle0), peerCloseIsError(peerCloseIsError0)
{
    createHelpers();
}

MessagingHandler::MessagingHandler(bool rawHandler, int prefetch0, bool autoAccept0, bool autoSettle0,
                                   bool peerCloseIsError0) :
    prefetch(prefetch0), autoAccept(autoAccept0), autoSettle(autoSettle0), peerCloseIsError(peerCloseIsError0)
{
    if (rawHandler) {
        flowController = 0;
        messagingAdapter = 0;
    } else {
        createHelpers();
    }
}

void MessagingHandler::createHelpers() {
    if (prefetch > 0) {
        flowController = new CFlowController(prefetch);
        addChildHandler(*flowController);
    }
    messagingAdapter = new MessagingAdapter(*this);
    addChildHandler(*messagingAdapter);
}

MessagingHandler::~MessagingHandler(){
    delete flowController;
    delete messagingAdapter;
};

void MessagingHandler::onAbort(Event &e) { onUnhandled(e); }
void MessagingHandler::onAccepted(Event &e) { onUnhandled(e); }
void MessagingHandler::onCommit(Event &e) { onUnhandled(e); }
void MessagingHandler::onConnectionClosed(Event &e) { onUnhandled(e); }
void MessagingHandler::onConnectionClosing(Event &e) { onUnhandled(e); }
void MessagingHandler::onConnectionError(Event &e) { onUnhandled(e); }
void MessagingHandler::onConnectionOpened(Event &e) { onUnhandled(e); }
void MessagingHandler::onConnectionOpening(Event &e) { onUnhandled(e); }
void MessagingHandler::onDisconnected(Event &e) { onUnhandled(e); }
void MessagingHandler::onFetch(Event &e) { onUnhandled(e); }
void MessagingHandler::onIdLoaded(Event &e) { onUnhandled(e); }
void MessagingHandler::onLinkClosed(Event &e) { onUnhandled(e); }
void MessagingHandler::onLinkClosing(Event &e) { onUnhandled(e); }
void MessagingHandler::onLinkError(Event &e) { onUnhandled(e); }
void MessagingHandler::onLinkOpened(Event &e) { onUnhandled(e); }
void MessagingHandler::onLinkOpening(Event &e) { onUnhandled(e); }
void MessagingHandler::onMessage(Event &e) { onUnhandled(e); }
void MessagingHandler::onQuit(Event &e) { onUnhandled(e); }
void MessagingHandler::onRecordInserted(Event &e) { onUnhandled(e); }
void MessagingHandler::onRecordsLoaded(Event &e) { onUnhandled(e); }
void MessagingHandler::onRejected(Event &e) { onUnhandled(e); }
void MessagingHandler::onReleased(Event &e) { onUnhandled(e); }
void MessagingHandler::onRequest(Event &e) { onUnhandled(e); }
void MessagingHandler::onResponse(Event &e) { onUnhandled(e); }
void MessagingHandler::onSendable(Event &e) { onUnhandled(e); }
void MessagingHandler::onSessionClosed(Event &e) { onUnhandled(e); }
void MessagingHandler::onSessionClosing(Event &e) { onUnhandled(e); }
void MessagingHandler::onSessionError(Event &e) { onUnhandled(e); }
void MessagingHandler::onSessionOpened(Event &e) { onUnhandled(e); }
void MessagingHandler::onSessionOpening(Event &e) { onUnhandled(e); }
void MessagingHandler::onSettled(Event &e) { onUnhandled(e); }
void MessagingHandler::onStart(Event &e) { onUnhandled(e); }
void MessagingHandler::onTimer(Event &e) { onUnhandled(e); }
void MessagingHandler::onTransactionAborted(Event &e) { onUnhandled(e); }
void MessagingHandler::onTransactionCommitted(Event &e) { onUnhandled(e); }
void MessagingHandler::onTransactionDeclared(Event &e) { onUnhandled(e); }
void MessagingHandler::onTransportClosed(Event &e) { onUnhandled(e); }

}} // namespace proton::reactor
