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

#include "proton/cpp/ProtonHandler.h"
#include "proton/event.h"

namespace proton {
namespace reactor {

class Event;

class PROTON_CPP_EXTERN MessagingHandler : public ProtonHandler
{
  public:
    PROTON_CPP_EXTERN MessagingHandler();
    virtual ~MessagingHandler();

    virtual void onAbort(Event &e);
    virtual void onAccepted(Event &e);
    virtual void onCommit(Event &e);
    virtual void onConnectionClose(Event &e);
    virtual void onConnectionClosed(Event &e);
    virtual void onConnectionClosing(Event &e);
    virtual void onConnectionOpen(Event &e);
    virtual void onConnectionOpened(Event &e);
    virtual void onDisconnected(Event &e);
    virtual void onFetch(Event &e);
    virtual void onIdLoaded(Event &e);
    virtual void onLinkClosing(Event &e);
    virtual void onLinkOpened(Event &e);
    virtual void onLinkOpening(Event &e);
    virtual void onMessage(Event &e);
    virtual void onQuit(Event &e);
    virtual void onRecordInserted(Event &e);
    virtual void onRecordsLoaded(Event &e);
    virtual void onRejected(Event &e);
    virtual void onReleased(Event &e);
    virtual void onRequest(Event &e);
    virtual void onResponse(Event &e);
    virtual void onSendable(Event &e);
    virtual void onSettled(Event &e);
    virtual void onStart(Event &e);
    virtual void onTimer(Event &e);
    virtual void onTransactionAborted(Event &e);
    virtual void onTransactionCommitted(Event &e);
    virtual void onTransactionDeclared(Event &e);
};


}} // namespace proton::reactor

#endif  /*!PROTON_CPP_MESSAGING_HANDLER_H*/
