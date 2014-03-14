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
package org.apache.qpid.proton.systemtests;

import static org.junit.Assert.assertEquals;

import org.apache.qpid.proton.ProtonFactoryLoader;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.EngineFactory;
import org.apache.qpid.proton.engine.Transport;
import org.junit.Test;

public class SimpleTest
{

    @Test
    public void test()
    {
        EngineFactory engineFactory = new ProtonFactoryLoader<EngineFactory>(EngineFactory.class).loadFactory();

        Connection connection1 = engineFactory.createConnection();
        Connection connection2 = engineFactory.createConnection();;
        Transport transport1 = engineFactory.createTransport();
        transport1.bind(connection1);

        Transport transport2 = engineFactory.createTransport();
        transport2.bind(connection2);

        assertEquals(EndpointState.UNINITIALIZED, connection1.getLocalState());
        assertEquals(EndpointState.UNINITIALIZED, connection1.getRemoteState());

        connection1.open();
        connection2.open();
    }


}
