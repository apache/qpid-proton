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
    public Sequence<? extends Endpoint> endpoints(EnumSet<EndpointState> local, EnumSet<EndpointState> remote);

    public Sequence<? extends Delivery> getWorkSequence();


}
