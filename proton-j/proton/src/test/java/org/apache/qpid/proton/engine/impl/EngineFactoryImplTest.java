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

import static org.junit.Assert.assertSame;
import static org.mockito.Mockito.mock;

import org.apache.qpid.proton.engine.EngineFactory;
import org.apache.qpid.proton.engine.EngineLogger;
import org.junit.Before;
import org.junit.Test;

public class EngineFactoryImplTest
{
    private final EngineLogger _engineLogger = mock(EngineLogger.class);

    private final EngineFactory _engineFactory = new EngineFactoryImpl();

    @Before
    public void setUp()
    {
        _engineFactory.setEngineLogger(_engineLogger);
    }

    @Test
    public void testCreateConnection_usesLogger()
    {

        assertSame(
                _engineLogger,
                _engineFactory.createConnection().getEngineLogger());
    }

    @Test
    public void testCreateTransport_usesLogger()
    {
        assertSame(
                _engineLogger,
                _engineFactory.createTransport().getEngineLogger());
    }
}
