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
#include "proton/cpp/Event.h"

namespace proton {
namespace reactor {

MessagingHandler::MessagingHandler(){};
MessagingHandler::~MessagingHandler(){};

void MessagingHandler::onAbort(Event &e) { onUnhandled(e); }
void MessagingHandler::onAccepted(Event &e) { onUnhandled(e); }
void MessagingHandler::onCommit(Event &e) { onUnhandled(e); }
void MessagingHandler::onConnectionClose(Event &e) { onUnhandled(e); }
void MessagingHandler::onConnectionClosed(Event &e) { onUnhandled(e); }
void MessagingHandler::onConnectionClosing(Event &e) { onUnhandled(e); }
void MessagingHandler::onConnectionOpen(Event &e) { onUnhandled(e); }
void MessagingHandler::onConnectionOpened(Event &e) { onUnhandled(e); }
void MessagingHandler::onDisconnected(Event &e) { onUnhandled(e); }
void MessagingHandler::onFetch(Event &e) { onUnhandled(e); }
void MessagingHandler::onIdLoaded(Event &e) { onUnhandled(e); }
void MessagingHandler::onLinkClosing(Event &e) { onUnhandled(e); }
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
void MessagingHandler::onSettled(Event &e) { onUnhandled(e); }
void MessagingHandler::onStart(Event &e) { onUnhandled(e); }
void MessagingHandler::onTimer(Event &e) { onUnhandled(e); }
void MessagingHandler::onTransactionAborted(Event &e) { onUnhandled(e); }
void MessagingHandler::onTransactionCommitted(Event &e) { onUnhandled(e); }
void MessagingHandler::onTransactionDeclared(Event &e) { onUnhandled(e); }

}} // namespace proton::reactor
