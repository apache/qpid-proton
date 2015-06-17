#ifndef PROTON_CPP_MESSAGING_HANDLER_H
#define PROTON_CPP_MESSAGING_HANDLER_H

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

#include "proton/ProtonHandler.hpp"
#include "proton/Acking.hpp"
#include "proton/event.h"

namespace proton {
namespace reactor {

class Event;
class MessagingAdapter;

class MessagingHandler : public ProtonHandler , public Acking
{
  public:
    PN_CPP_EXTERN MessagingHandler(int prefetch=10, bool autoAccept=true, bool autoSettle=true,
                                       bool peerCloseIsError=false);
    PN_CPP_EXTERN virtual ~MessagingHandler();

    PN_CPP_EXTERN virtual void onAbort(Event &e);
    PN_CPP_EXTERN virtual void onAccepted(Event &e);
    PN_CPP_EXTERN virtual void onCommit(Event &e);
    PN_CPP_EXTERN virtual void onConnectionClosed(Event &e);
    PN_CPP_EXTERN virtual void onConnectionClosing(Event &e);
    PN_CPP_EXTERN virtual void onConnectionError(Event &e);
    PN_CPP_EXTERN virtual void onConnectionOpening(Event &e);
    PN_CPP_EXTERN virtual void onConnectionOpened(Event &e);
    PN_CPP_EXTERN virtual void onDisconnected(Event &e);
    PN_CPP_EXTERN virtual void onFetch(Event &e);
    PN_CPP_EXTERN virtual void onIdLoaded(Event &e);
    PN_CPP_EXTERN virtual void onLinkClosed(Event &e);
    PN_CPP_EXTERN virtual void onLinkClosing(Event &e);
    PN_CPP_EXTERN virtual void onLinkError(Event &e);
    PN_CPP_EXTERN virtual void onLinkOpened(Event &e);
    PN_CPP_EXTERN virtual void onLinkOpening(Event &e);
    PN_CPP_EXTERN virtual void onMessage(Event &e);
    PN_CPP_EXTERN virtual void onQuit(Event &e);
    PN_CPP_EXTERN virtual void onRecordInserted(Event &e);
    PN_CPP_EXTERN virtual void onRecordsLoaded(Event &e);
    PN_CPP_EXTERN virtual void onRejected(Event &e);
    PN_CPP_EXTERN virtual void onReleased(Event &e);
    PN_CPP_EXTERN virtual void onRequest(Event &e);
    PN_CPP_EXTERN virtual void onResponse(Event &e);
    PN_CPP_EXTERN virtual void onSendable(Event &e);
    PN_CPP_EXTERN virtual void onSessionClosed(Event &e);
    PN_CPP_EXTERN virtual void onSessionClosing(Event &e);
    PN_CPP_EXTERN virtual void onSessionError(Event &e);
    PN_CPP_EXTERN virtual void onSessionOpened(Event &e);
    PN_CPP_EXTERN virtual void onSessionOpening(Event &e);
    PN_CPP_EXTERN virtual void onSettled(Event &e);
    PN_CPP_EXTERN virtual void onStart(Event &e);
    PN_CPP_EXTERN virtual void onTimer(Event &e);
    PN_CPP_EXTERN virtual void onTransactionAborted(Event &e);
    PN_CPP_EXTERN virtual void onTransactionCommitted(Event &e);
    PN_CPP_EXTERN virtual void onTransactionDeclared(Event &e);
    PN_CPP_EXTERN virtual void onTransportClosed(Event &e);

 protected:
    int prefetch;
    bool autoAccept;
    bool autoSettle;
    bool peerCloseIsError;
    MessagingAdapter *messagingAdapter;
    Handler *flowController;
    PN_CPP_EXTERN MessagingHandler(
        bool rawHandler, int prefetch=10, bool autoAccept=true,
        bool autoSettle=true, bool peerCloseIsError=false);
  private:
    friend class ContainerImpl;
    friend class MessagingAdapter;
    PN_CPP_EXTERN void createHelpers();
};

}}

#endif  /*!PROTON_CPP_MESSAGING_HANDLER_H*/
