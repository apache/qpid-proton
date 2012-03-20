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

import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.type.UnsignedInteger;

public class TransportDelivery
{
    private UnsignedInteger _deliveryId;
    private Delivery _delivery;
    private TransportLink _transportLink;

    public TransportDelivery(UnsignedInteger currentDeliveryId, Delivery delivery, TransportLink transportLink)
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
}
