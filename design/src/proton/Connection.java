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
 * Connection
 *
 * @opt operations
 * @opt types
 *
 * @composed 1 - "0..n" Session
 * @composed 1 - "0..?" Transport
 *
 */

public interface Connection extends Endpoint
{

    /**
     * transition local state to ACTIVE
     */
    public void open();

    /**
     * transition local state to CLOSED
     */
    public void close();

    /**
     * @return a newly created session
     */
    public Session session();

    /**
     * @return a newly created transport
     */
    public Transport transport();

    /**
     * @return iterator for endpoints matching the specified local and
     *         remote states
     */
    public Iterator<Endpoint> endpoints(Endpoint.State local, Endpoint.State remote);

    /**
     * @return iterator for incoming link endpoints with pending
     *         transfers
     */
    public Iterator<Receiver> incoming();

    /**
     * @return iterator for unblocked outgoing link endpoints with
     *         offered credits
     */
    public Iterator<Sender> outgoing();

}
