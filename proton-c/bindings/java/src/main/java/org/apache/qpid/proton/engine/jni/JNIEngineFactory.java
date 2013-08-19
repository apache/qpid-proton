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
package org.apache.qpid.proton.engine.jni;

import org.apache.qpid.proton.ProtonCEquivalent;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.EngineFactory;
import org.apache.qpid.proton.engine.SslDomain;
import org.apache.qpid.proton.engine.SslPeerDetails;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.jni.JNIFactory;

public class JNIEngineFactory extends JNIFactory implements EngineFactory
{
    @Override
    @ProtonCEquivalent("pn_connection")
    public Connection createConnection()
    {
        return new JNIConnection();
    }

    @Override
    public Transport createTransport()
    {
        return new JNITransport();
    }

    @Override
    public SslDomain createSslDomain()
    {
        return new JNISslDomain();
    }

    @Override
    public SslPeerDetails createSslPeerDetails(final String hostname, final int port)
    {
        return new SslPeerDetails()
        {

            @Override
            public int getPort()
            {
                return port;
            }

            @Override
            public String getHostname()
            {
                return hostname;
            }
        };
    }
}
