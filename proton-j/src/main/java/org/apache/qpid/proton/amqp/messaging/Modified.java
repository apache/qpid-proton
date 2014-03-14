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

package org.apache.qpid.proton.amqp.messaging;

import java.util.Map;
import org.apache.qpid.proton.amqp.transport.DeliveryState;

public final class Modified
      implements DeliveryState, Outcome
{

    private Boolean _deliveryFailed;
    private Boolean _undeliverableHere;
    private Map _messageAnnotations;

    public Boolean getDeliveryFailed()
    {
        return _deliveryFailed;
    }

    public void setDeliveryFailed(Boolean deliveryFailed)
    {
        _deliveryFailed = deliveryFailed;
    }

    public Boolean getUndeliverableHere()
    {
        return _undeliverableHere;
    }

    public void setUndeliverableHere(Boolean undeliverableHere)
    {
        _undeliverableHere = undeliverableHere;
    }

    public Map getMessageAnnotations()
    {
        return _messageAnnotations;
    }

    public void setMessageAnnotations(Map messageAnnotations)
    {
        _messageAnnotations = messageAnnotations;
    }

    @Override
    public String toString()
    {
        return "Modified{" +
               "deliveryFailed=" + _deliveryFailed +
               ", undeliverableHere=" + _undeliverableHere +
               ", messageAnnotations=" + _messageAnnotations +
               '}';
    }
}
  