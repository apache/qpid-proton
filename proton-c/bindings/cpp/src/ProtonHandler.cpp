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
#include "proton/cpp/ProtonEvent.h"

namespace proton {
namespace reactor {

ProtonHandler::ProtonHandler(){};

// Everything goes to onUnhandled() unless overriden by subclass

void ProtonHandler::onReactorInit(Event &e) { onUnhandled(e); }
void ProtonHandler::onReactorQuiesced(Event &e) { onUnhandled(e); }
void ProtonHandler::onReactorFinal(Event &e) { onUnhandled(e); }
void ProtonHandler::onTimerTask(Event &e) { onUnhandled(e); }
void ProtonHandler::onConnectionInit(Event &e) { onUnhandled(e); }
void ProtonHandler::onConnectionBound(Event &e) { onUnhandled(e); }
void ProtonHandler::onConnectionUnbound(Event &e) { onUnhandled(e); }
void ProtonHandler::onConnectionLocalOpen(Event &e) { onUnhandled(e); }
void ProtonHandler::onConnectionLocalClose(Event &e) { onUnhandled(e); }
void ProtonHandler::onConnectionRemoteOpen(Event &e) { onUnhandled(e); }
void ProtonHandler::onConnectionRemoteClose(Event &e) { onUnhandled(e); }
void ProtonHandler::onConnectionFinal(Event &e) { onUnhandled(e); }
void ProtonHandler::onSessionInit(Event &e) { onUnhandled(e); }
void ProtonHandler::onSessionLocalOpen(Event &e) { onUnhandled(e); }
void ProtonHandler::onSessionLocalClose(Event &e) { onUnhandled(e); }
void ProtonHandler::onSessionRemoteOpen(Event &e) { onUnhandled(e); }
void ProtonHandler::onSessionRemoteClose(Event &e) { onUnhandled(e); }
void ProtonHandler::onSessionFinal(Event &e) { onUnhandled(e); }
void ProtonHandler::onLinkInit(Event &e) { onUnhandled(e); }
void ProtonHandler::onLinkLocalOpen(Event &e) { onUnhandled(e); }
void ProtonHandler::onLinkLocalClose(Event &e) { onUnhandled(e); }
void ProtonHandler::onLinkLocalDetach(Event &e) { onUnhandled(e); }
void ProtonHandler::onLinkRemoteOpen(Event &e) { onUnhandled(e); }
void ProtonHandler::onLinkRemoteClose(Event &e) { onUnhandled(e); }
void ProtonHandler::onLinkRemoteDetach(Event &e) { onUnhandled(e); }
void ProtonHandler::onLinkFlow(Event &e) { onUnhandled(e); }
void ProtonHandler::onLinkFinal(Event &e) { onUnhandled(e); }
void ProtonHandler::onDelivery(Event &e) { onUnhandled(e); }
void ProtonHandler::onTransport(Event &e) { onUnhandled(e); }
void ProtonHandler::onTransportError(Event &e) { onUnhandled(e); }
void ProtonHandler::onTransportHeadClosed(Event &e) { onUnhandled(e); }
void ProtonHandler::onTransportTailClosed(Event &e) { onUnhandled(e); }
void ProtonHandler::onTransportClosed(Event &e) { onUnhandled(e); }
void ProtonHandler::onSelectableInit(Event &e) { onUnhandled(e); }
void ProtonHandler::onSelectableUpdated(Event &e) { onUnhandled(e); }
void ProtonHandler::onSelectableReadable(Event &e) { onUnhandled(e); }
void ProtonHandler::onSelectableWritable(Event &e) { onUnhandled(e); }
void ProtonHandler::onSelectableExpired(Event &e) { onUnhandled(e); }
void ProtonHandler::onSelectableError(Event &e) { onUnhandled(e); }
void ProtonHandler::onSelectableFinal(Event &e) { onUnhandled(e); }

void ProtonHandler::onUnhandled(Event &e) {}

}} // namespace proton::reactor
