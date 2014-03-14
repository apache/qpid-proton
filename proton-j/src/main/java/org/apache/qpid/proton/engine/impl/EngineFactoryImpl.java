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
package org.apache.qpid.proton.engine.impl;

import org.apache.qpid.proton.ProtonFactoryImpl;
import org.apache.qpid.proton.engine.EngineFactory;
import org.apache.qpid.proton.engine.ProtonJConnection;
import org.apache.qpid.proton.engine.ProtonJSslDomain;
import org.apache.qpid.proton.engine.ProtonJSslPeerDetails;
import org.apache.qpid.proton.engine.ProtonJTransport;
import org.apache.qpid.proton.engine.impl.ssl.SslDomainImpl;
import org.apache.qpid.proton.engine.impl.ssl.SslPeerDetailsImpl;

public class EngineFactoryImpl extends ProtonFactoryImpl implements EngineFactory
{
    @SuppressWarnings("deprecation") // TODO remove once the constructor is made non-public (and therefore non-deprecated)
    @Override
    public ProtonJConnection createConnection()
    {
        return new ConnectionImpl();
    }

    @SuppressWarnings("deprecation") // TODO remove once the constructor is made non-public (and therefore non-deprecated)
    @Override
    public ProtonJTransport createTransport()
    {
        return new TransportImpl();
    }

    @SuppressWarnings("deprecation") // TODO remove once the constructor is made non-public (and therefore non-deprecated)
    @Override
    public ProtonJSslDomain createSslDomain()
    {
        return new SslDomainImpl();
    }

    @SuppressWarnings("deprecation") // TODO remove once the constructor is made non-public (and therefore non-deprecated)
    @Override
    public ProtonJSslPeerDetails createSslPeerDetails(String hostname, int port)
    {
        return new SslPeerDetailsImpl(hostname, port);
    }
}
