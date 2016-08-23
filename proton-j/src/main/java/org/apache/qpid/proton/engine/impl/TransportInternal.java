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

import org.apache.qpid.proton.engine.Transport;

/**
 * Extended Transport interface providing access to certain methods intended mainly for internal
 * use, or use in extending implementation details not strictly considered part of the public
 * Transport API.
 */
public interface TransportInternal extends Transport
{
    /**
     * Add a {@link TransportLayer} to the transport, wrapping the input and output process handlers
     * in the state they currently exist. No effect if the given layer was previously added.
     *
     * @param layer the layer to add (if it was not previously added)
     * @throws IllegalStateException if processing has already started.
     */
    void addTransportLayer(TransportLayer layer) throws IllegalStateException;
}
