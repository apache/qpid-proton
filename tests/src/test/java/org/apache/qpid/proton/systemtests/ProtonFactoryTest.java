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

import static org.junit.Assert.assertNotNull;

import org.apache.qpid.proton.ProtonFactoryLoader;
import org.apache.qpid.proton.engine.EngineFactory;
import org.apache.qpid.proton.message.MessageFactory;
import org.apache.qpid.proton.messenger.MessengerFactory;
import org.junit.Test;

public class ProtonFactoryTest
{
    @SuppressWarnings({ "rawtypes", "unchecked" })
    @Test
    public void testLoadFactoryWithExplicitClass()
    {
        ProtonFactoryLoader factoryLoader = new ProtonFactoryLoader();
        MessageFactory messageFactory = (MessageFactory) factoryLoader.loadFactory(MessageFactory.class);
        assertNotNull(messageFactory);
    }

    @Test
    public void testMessageFactory()
    {
        ProtonFactoryLoader<MessageFactory> factoryLoader = new ProtonFactoryLoader<MessageFactory>(MessageFactory.class);
        assertNotNull(factoryLoader.loadFactory());
    }

    @Test
    public void testEngineFactory()
    {
        ProtonFactoryLoader<EngineFactory> factoryLoader = new ProtonFactoryLoader<EngineFactory>(EngineFactory.class);
        assertNotNull(factoryLoader.loadFactory());
    }

    @Test
    public void testMessengerFactory()
    {
        ProtonFactoryLoader<MessengerFactory> factoryLoader = new ProtonFactoryLoader<MessengerFactory>(MessengerFactory.class);
        assertNotNull(factoryLoader.loadFactory());
    }
}
