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


/**
 * Event
 *
 */

public interface Event
{

    public enum Type {
        CONNECTION_INIT,
        CONNECTION_BOUND,
        CONNECTION_UNBOUND,
        CONNECTION_OPEN,
        CONNECTION_REMOTE_OPEN,
        CONNECTION_CLOSE,
        CONNECTION_REMOTE_CLOSE,
        CONNECTION_FINAL,

        SESSION_INIT,
        SESSION_OPEN,
        SESSION_REMOTE_OPEN,
        SESSION_CLOSE,
        SESSION_REMOTE_CLOSE,
        SESSION_FINAL,

        LINK_INIT,
        LINK_OPEN,
        LINK_REMOTE_OPEN,
        LINK_CLOSE,
        LINK_REMOTE_CLOSE,
        LINK_DETACH,
        LINK_REMOTE_DETACH,
        LINK_FLOW,
        LINK_FINAL,

        DELIVERY,

        TRANSPORT,
        TRANSPORT_ERROR,
        TRANSPORT_HEAD_CLOSED,
        TRANSPORT_TAIL_CLOSED,
        TRANSPORT_CLOSED
    }

    Type getType();

    Object getContext();

    Connection getConnection();

    Session getSession();

    Link getLink();

    Delivery getDelivery();

    Transport getTransport();

    Event copy();

}
