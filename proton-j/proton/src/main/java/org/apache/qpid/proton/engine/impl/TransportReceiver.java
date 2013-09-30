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

package org.apache.qpid.proton.engine.impl;

import org.apache.qpid.proton.amqp.transport.Flow;

class TransportReceiver extends TransportLink<ReceiverImpl>
{


    TransportReceiver(ReceiverImpl link)
    {
        super(link);
        link.setTransportLink(this);
    }

    public ReceiverImpl getReceiver()
    {
        return getLink();
    }

    @Override
    void handleFlow(Flow flow)
    {
        super.handleFlow(flow);
        int remote = getRemoteDeliveryCount().intValue();
        int local = getDeliveryCount().intValue();
        int delta = remote - local;
        if(delta > 0)
        {
            getLink().addCredit(-delta);
            setLinkCredit(getRemoteLinkCredit());
            setDeliveryCount(getRemoteDeliveryCount());
            getLink().setDrained(getLink().getDrained() + delta);
        }


    }
}
