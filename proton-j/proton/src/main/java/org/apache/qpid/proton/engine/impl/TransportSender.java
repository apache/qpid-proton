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

import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.amqp.transport.Flow;

class TransportSender extends TransportLink<SenderImpl>
{
    private boolean _drain;
    private static final UnsignedInteger ORIGINAL_DELIVERY_COUNT = UnsignedInteger.ZERO;

    TransportSender(SenderImpl link)
    {
        super(link);
        setDeliveryCount(ORIGINAL_DELIVERY_COUNT);
        link.setTransportLink(this);
    }

    @Override
    void handleFlow(Flow flow)
    {
        super.handleFlow(flow);
        _drain = flow.getDrain();
        getLink().setDrain(flow.getDrain());
        int oldCredit = getLink().getCredit();
        UnsignedInteger oldLimit = getLinkCredit().add(getDeliveryCount());
        UnsignedInteger transferLimit = flow.getLinkCredit().add(flow.getDeliveryCount() == null
                                                                         ? ORIGINAL_DELIVERY_COUNT
                                                                         : flow.getDeliveryCount());
        UnsignedInteger linkCredit = transferLimit.subtract(getDeliveryCount());

        setLinkCredit(linkCredit);
        getLink().setCredit(transferLimit.subtract(oldLimit).intValue() + oldCredit);

        DeliveryImpl current = getLink().current();
        getLink().getConnectionImpl().workUpdate(current);
        setLinkCredit(linkCredit);
    }

}
