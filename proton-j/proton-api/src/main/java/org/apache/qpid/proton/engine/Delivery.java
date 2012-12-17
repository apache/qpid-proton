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
package org.apache.qpid.proton.engine;

import org.apache.qpid.proton.amqp.transport.DeliveryState;

/**
 * Delivery
 *
 */

public interface Delivery
{

    public byte[] getTag();

    public Link getLink();

    public DeliveryState getLocalState();

    public DeliveryState getRemoteState();

    public boolean remotelySettled();

    public int getMessageFormat();

    /**
     * updates the state of the delivery
     *
     * @param state the new delivery state
     */
    public void disposition(DeliveryState state);

    /**
     */
    public void settle();

    public boolean isSettled();

    public void free();

    public Delivery getWorkNext();

    public boolean isWritable();

    public boolean isReadable();

    public void setContext(Object o);

    public Object getContext();

    public boolean isUpdated();
}
