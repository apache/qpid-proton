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

public class TransportDelivery
{
    private UnsignedInteger _deliveryId;
    private DeliveryImpl _delivery;
    private TransportLink _transportLink;
    private int _sessionSize = 1;

    TransportDelivery(UnsignedInteger currentDeliveryId, DeliveryImpl delivery, TransportLink transportLink)
    {
        _deliveryId = currentDeliveryId;
        _delivery = delivery;
        _transportLink = transportLink;
    }

    public UnsignedInteger getDeliveryId()
    {
        return _deliveryId;
    }

    public TransportLink getTransportLink()
    {
        return _transportLink;
    }

    void incrementSessionSize()
    {
        _sessionSize++;
    }

    int getSessionSize()
    {
        return _sessionSize;
    }

    void settled()
    {
        _transportLink.settled(this);
        _delivery.updateWork();
    }
}
