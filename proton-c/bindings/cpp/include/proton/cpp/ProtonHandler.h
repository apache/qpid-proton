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
#include "proton/cpp/Handler.h"

namespace proton {
namespace reactor {

class Event;
class ProtonEvent;

class ProtonHandler : public Handler
{
  public:
    PROTON_CPP_EXTERN ProtonHandler();
    virtual void onReactorInit(Event &e);
    virtual void onReactorQuiesced(Event &e);
    virtual void onReactorFinal(Event &e);
    virtual void onTimerTask(Event &e);
    virtual void onConnectionInit(Event &e);
    virtual void onConnectionBound(Event &e);
    virtual void onConnectionUnbound(Event &e);
    virtual void onConnectionLocalOpen(Event &e);
    virtual void onConnectionLocalClose(Event &e);
    virtual void onConnectionRemoteOpen(Event &e);
    virtual void onConnectionRemoteClose(Event &e);
    virtual void onConnectionFinal(Event &e);
    virtual void onSessionInit(Event &e);
    virtual void onSessionLocalOpen(Event &e);
    virtual void onSessionLocalClose(Event &e);
    virtual void onSessionRemoteOpen(Event &e);
    virtual void onSessionRemoteClose(Event &e);
    virtual void onSessionFinal(Event &e);
    virtual void onLinkInit(Event &e);
    virtual void onLinkLocalOpen(Event &e);
    virtual void onLinkLocalClose(Event &e);
    virtual void onLinkLocalDetach(Event &e);
    virtual void onLinkRemoteOpen(Event &e);
    virtual void onLinkRemoteClose(Event &e);
    virtual void onLinkRemoteDetach(Event &e);
    virtual void onLinkFlow(Event &e);
    virtual void onLinkFinal(Event &e);
    virtual void onDelivery(Event &e);
    virtual void onTransport(Event &e);
    virtual void onTransportError(Event &e);
    virtual void onTransportHeadClosed(Event &e);
    virtual void onTransportTailClosed(Event &e);
    virtual void onTransportClosed(Event &e);
    virtual void onSelectableInit(Event &e);
    virtual void onSelectableUpdated(Event &e);
    virtual void onSelectableReadable(Event &e);
    virtual void onSelectableWritable(Event &e);
    virtual void onSelectableExpired(Event &e);
    virtual void onSelectableError(Event &e);
    virtual void onSelectableFinal(Event &e);

    virtual void onUnhandled(Event &e);
};


}} // namespace proton::reactor

#endif  /*!PROTON_CPP_PROTONHANDLER_H*/
