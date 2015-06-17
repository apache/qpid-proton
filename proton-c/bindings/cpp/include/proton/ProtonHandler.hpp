#ifndef PROTON_CPP_PROTONHANDLER_H
#define PROTON_CPP_PROTONHANDLER_H

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
#include "proton/Handler.hpp"

namespace proton {
namespace reactor {

class Event;
class ProtonEvent;

class ProtonHandler : public Handler
{
  public:
    PN_CPP_EXTERN ProtonHandler();
    PN_CPP_EXTERN virtual void onReactorInit(Event &e);
    PN_CPP_EXTERN virtual void onReactorQuiesced(Event &e);
    PN_CPP_EXTERN virtual void onReactorFinal(Event &e);
    PN_CPP_EXTERN virtual void onTimerTask(Event &e);
    PN_CPP_EXTERN virtual void onConnectionInit(Event &e);
    PN_CPP_EXTERN virtual void onConnectionBound(Event &e);
    PN_CPP_EXTERN virtual void onConnectionUnbound(Event &e);
    PN_CPP_EXTERN virtual void onConnectionLocalOpen(Event &e);
    PN_CPP_EXTERN virtual void onConnectionLocalClose(Event &e);
    PN_CPP_EXTERN virtual void onConnectionRemoteOpen(Event &e);
    PN_CPP_EXTERN virtual void onConnectionRemoteClose(Event &e);
    PN_CPP_EXTERN virtual void onConnectionFinal(Event &e);
    PN_CPP_EXTERN virtual void onSessionInit(Event &e);
    PN_CPP_EXTERN virtual void onSessionLocalOpen(Event &e);
    PN_CPP_EXTERN virtual void onSessionLocalClose(Event &e);
    PN_CPP_EXTERN virtual void onSessionRemoteOpen(Event &e);
    PN_CPP_EXTERN virtual void onSessionRemoteClose(Event &e);
    PN_CPP_EXTERN virtual void onSessionFinal(Event &e);
    PN_CPP_EXTERN virtual void onLinkInit(Event &e);
    PN_CPP_EXTERN virtual void onLinkLocalOpen(Event &e);
    PN_CPP_EXTERN virtual void onLinkLocalClose(Event &e);
    PN_CPP_EXTERN virtual void onLinkLocalDetach(Event &e);
    PN_CPP_EXTERN virtual void onLinkRemoteOpen(Event &e);
    PN_CPP_EXTERN virtual void onLinkRemoteClose(Event &e);
    PN_CPP_EXTERN virtual void onLinkRemoteDetach(Event &e);
    PN_CPP_EXTERN virtual void onLinkFlow(Event &e);
    PN_CPP_EXTERN virtual void onLinkFinal(Event &e);
    PN_CPP_EXTERN virtual void onDelivery(Event &e);
    PN_CPP_EXTERN virtual void onTransport(Event &e);
    PN_CPP_EXTERN virtual void onTransportError(Event &e);
    PN_CPP_EXTERN virtual void onTransportHeadClosed(Event &e);
    PN_CPP_EXTERN virtual void onTransportTailClosed(Event &e);
    PN_CPP_EXTERN virtual void onTransportClosed(Event &e);
    PN_CPP_EXTERN virtual void onSelectableInit(Event &e);
    PN_CPP_EXTERN virtual void onSelectableUpdated(Event &e);
    PN_CPP_EXTERN virtual void onSelectableReadable(Event &e);
    PN_CPP_EXTERN virtual void onSelectableWritable(Event &e);
    PN_CPP_EXTERN virtual void onSelectableExpired(Event &e);
    PN_CPP_EXTERN virtual void onSelectableError(Event &e);
    PN_CPP_EXTERN virtual void onSelectableFinal(Event &e);

    PN_CPP_EXTERN virtual void onUnhandled(Event &e);
};

}}

#endif  /*!PROTON_CPP_PROTONHANDLER_H*/
