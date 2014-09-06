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
 * Engine
 *
 */

public final class Engine
{

    private Engine()
    {
    }

    public static Collector collector()
    {
        return Collector.Factory.create();
    }

    public static Connection connection()
    {
        return Connection.Factory.create();
    }

    public static Transport transport()
    {
        return Transport.Factory.create();
    }

    public static SslDomain sslDomain()
    {
        return SslDomain.Factory.create();
    }

    public static SslPeerDetails sslPeerDetails(String hostname, int port)
    {
        return SslPeerDetails.Factory.create(hostname, port);
    }

}
