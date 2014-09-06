/*
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
 */
package org.apache.qpid.proton.engine;

import org.apache.qpid.proton.engine.impl.ssl.SslPeerDetailsImpl;

/**
 * The details of the remote peer involved in an SSL session.
 *
 * Used when creating an SSL session to hint that the underlying SSL implementation
 * should attempt to resume a previous session if one exists for the same peer details,
 * e.g. using session identifiers (http://tools.ietf.org/html/rfc5246) or session tickets
 * (http://tools.ietf.org/html/rfc5077).
 */
public interface SslPeerDetails
{

    public static final class Factory
    {
        public static SslPeerDetails create(String hostname, int port) {
            return new SslPeerDetailsImpl(hostname, port);
        }
    }

    String getHostname();
    int getPort();
}
