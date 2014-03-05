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
import org.apache.qpid.proton.engine.Event;

class TransportLink<T extends LinkImpl>
{
    private UnsignedInteger _localHandle;
    private String _name;
    private UnsignedInteger _remoteHandle;
    private UnsignedInteger _deliveryCount;
    private UnsignedInteger _linkCredit = UnsignedInteger.ZERO;
    private T _link;
    private UnsignedInteger _remoteDeliveryCount;
    private UnsignedInteger _remoteLinkCredit;
    private boolean _detachReceived;
    private boolean _attachSent;

    protected TransportLink(T link)
    {
        _link = link;
        _name = link.getName();
    }

    static <L extends LinkImpl> TransportLink<L> createTransportLink(L link)
    {
        return (TransportLink<L>) (link instanceof ReceiverImpl
                       ? new TransportReceiver((ReceiverImpl)link)
                       : new TransportSender((SenderImpl)link));
    }

    public UnsignedInteger getLocalHandle()
    {
        return _localHandle;
    }

    public void setLocalHandle(UnsignedInteger localHandle)
    {
        _localHandle = localHandle;
    }

    public boolean isLocalHandleSet()
    {
        return _localHandle != null;
    }

    public String getName()
    {
        return _name;
    }

    public void setName(String name)
    {
        _name = name;
    }

    public void clearLocalHandle()
    {
        _localHandle = null;
    }

    public UnsignedInteger getRemoteHandle()
    {
        return _remoteHandle;
    }

    public void setRemoteHandle(UnsignedInteger remoteHandle)
    {
        _remoteHandle = remoteHandle;
    }

    public UnsignedInteger getDeliveryCount()
    {
        return _deliveryCount;
    }

    public UnsignedInteger getLinkCredit()
    {
        return _linkCredit;
    }

    public void addCredit(int credits)
    {
        _linkCredit = UnsignedInteger.valueOf(_linkCredit.intValue() + credits);
    }

    public boolean hasCredit()
    {
        return getLinkCredit().compareTo(UnsignedInteger.ZERO) > 0;
    }

    public T getLink()
    {
        return _link;
    }

    void handleFlow(Flow flow)
    {
        _remoteDeliveryCount = flow.getDeliveryCount();
        _remoteLinkCredit = flow.getLinkCredit();


        EventImpl ev = _link.getConnectionImpl().put(Event.Type.LINK_FLOW);
        if (ev != null) {
            ev.init(_link);
        }
    }

    void setLinkCredit(UnsignedInteger linkCredit)
    {
        _linkCredit = linkCredit;
    }

    public void setDeliveryCount(UnsignedInteger deliveryCount)
    {
        _deliveryCount = deliveryCount;
    }

    public void settled(TransportDelivery transportDelivery)
    {
        getLink().getSession().getTransportSession().settled(transportDelivery);
    }


    UnsignedInteger getRemoteDeliveryCount()
    {
        return _remoteDeliveryCount;
    }

    UnsignedInteger getRemoteLinkCredit()
    {
        return _remoteLinkCredit;
    }

    public void setRemoteLinkCredit(UnsignedInteger remoteLinkCredit)
    {
        _remoteLinkCredit = remoteLinkCredit;
    }

    void decrementLinkCredit()
    {
        _linkCredit = _linkCredit.subtract(UnsignedInteger.ONE);
    }

    void incrementDeliveryCount()
    {
        _deliveryCount = _deliveryCount.add(UnsignedInteger.ONE);
    }

    public void receivedDetach()
    {
        _detachReceived = true;
    }

    public boolean detachReceived()
    {
        return _detachReceived;
    }

    public boolean attachSent()
    {
        return _attachSent;
    }

    public void sentAttach()
    {
        _attachSent = true;
    }

    public void setRemoteDeliveryCount(UnsignedInteger remoteDeliveryCount)
    {
        _remoteDeliveryCount = remoteDeliveryCount;
    }
}
