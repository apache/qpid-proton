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
package proton;

import java.util.Iterator;


/**
 * Session
 *
 * @opt operations
 * @opt types
 *
 * @composed 1 - "0..n" Link
 * @composed 1 incoming 1 DeliveryBuffer
 * @composed 1 outgoing 1 DeliveryBuffer
 *
 */

public interface Session extends Endpoint
{

    /**
     * transition local state to ACTIVE
     */
    public void begin();

    /**
     * transition local state to CLOSED
     */
    public void end();

    /**
     * @return a newly created outgoing link
     */
    public Sender sender();

    /**
     * @return a newly created incoming link
     */
    public Receiver receiver();

    /**
     * @see Connection#endpoints(Endpoint.State, Endpoint.State)
     */
    public Iterator<Endpoint> endpoints(Endpoint.State local, Endpoint.State remote);

    /**
     * @see Connection#incoming()
     */
    public Iterator<Receiver> incoming();

    /**
     * @see Connection#outgoing()
     */
    public Iterator<Sender> outgoing();

}
