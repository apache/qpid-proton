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
 * Session
 *
 * Note that session level flow control is handled internally by Proton.
 */
public interface Session extends Endpoint
{
    /**
     * Returns a newly created sender endpoint
     */
    public Sender sender(String name);

    /**
     * Returns a newly created receiver endpoint
     */
    public Receiver receiver(String name);

    public Session next(EnumSet<EndpointState> local, EnumSet<EndpointState> remote);

    public Connection getConnection();

    public int getIncomingCapacity();

    public void setIncomingCapacity(int bytes);

    public int getIncomingBytes();

    public int getOutgoingBytes();

}
