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

import java.util.EnumSet;
import java.util.Map;
import org.apache.qpid.proton.amqp.Symbol;


/**
 * Maintains lists of sessions, links and deliveries in a state
 * that is interesting to the application.
 *
 * These are exposed by returning the head of those lists via
 * {@link #sessionHead(EnumSet, EnumSet)}, {@link #linkHead(EnumSet, EnumSet)}
 * {@link #getWorkHead()} respectively.
 */
public interface Connection extends Endpoint
{

    /**
     * Returns a newly created session
     *
     * TODO does the Connection's channel-max property limit how many sessions can be created,
     * or opened, or neither?
     */
    public Session session();

    /**
     * Returns the head of the list of sessions in the specified states.
     *
     * Typically used to discover sessions whose remote state has acquired
     * particular values, e.g. sessions that have been remotely opened or closed.
     *
     * TODO what ordering guarantees on the returned "linked list" are provided?
     *
     * @see Session#next(EnumSet, EnumSet)
     */
    public Session sessionHead(EnumSet<EndpointState> local, EnumSet<EndpointState> remote);

    /**
     * Returns the head of the list of links in the specified states.
     *
     * Typically used to discover links whose remote state has acquired
     * particular values, e.g. links that have been remotely opened or closed.
     *
     * @see Link#next(EnumSet, EnumSet)
     */
    public Link linkHead(EnumSet<EndpointState> local, EnumSet<EndpointState> remote);

    /**
     * Returns the head of the delivery work list. The delivery work list consists of
     * unsettled deliveries whose state has been changed by the other container
     * and not yet locally processed.
     *
     * @see Receiver#recv(byte[], int, int)
     * @see Delivery#settle()
     * @see Delivery#getWorkNext()
     */
    public Delivery getWorkHead();

    public void setContainer(String container);

    public void setHostname(String hostname);

    public String getRemoteContainer();

    public String getRemoteHostname();

    void setOfferedCapabilities(Symbol[] capabilities);

    void setDesiredCapabilities(Symbol[] capabilities);

    Symbol[] getRemoteOfferedCapabilities();

    Symbol[] getRemoteDesiredCapabilities();

    Map<Symbol,Object> getRemoteProperties();

    void setProperties(Map<Symbol,Object> properties);

    Object getContext();

    void setContext(Object context);

    void collect(Collector collector);

}
