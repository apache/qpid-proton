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
package org.apache.qpid.proton.systemtests.logging;


import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertThat;
import static org.junit.matchers.JUnitMatchers.containsString;

import java.nio.ByteBuffer;

import org.apache.qpid.proton.amqp.transport.Open;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.EngineFactory;
import org.apache.qpid.proton.logging.CategoryAwareProtonLogger;
import org.apache.qpid.proton.logging.ProtonCategoryLogger;
import org.apache.qpid.proton.logging.ProtonLogLevel;
import org.apache.qpid.proton.logging.ProtonLogger;
import org.apache.qpid.proton.logging.StdOutCategoryLogger;
import org.apache.qpid.proton.systemtests.engine.ProtonFactoryTestFixture;
import org.hamcrest.CoreMatchers;
import org.junit.After;
import org.junit.Ignore;
import org.junit.Test;

@Ignore("TODO reinstate test once proton-jni supports logging") // TODO reinstate test
public class LoggingCustomisationTest
{
    private EngineFactory _engineFactory = new ProtonFactoryTestFixture().getFactory1();
    private final RememberingLogger _customCategoryLogger = new RememberingLogger();

    @After
    public void restoreDefaultCategoryLogger()
    {
        ProtonLogger.setDefaultCategoryLogger(null);
    }

    @Test
    public void testDefaultCategoryLoggerUsesStdOut()
    {
        CategoryAwareProtonLogger engineLogger = (CategoryAwareProtonLogger)_engineFactory.getEngineLogger();
        assertThat(engineLogger.getCategoryLogger(), is(StdOutCategoryLogger.class));
    }

    @Test
    public void testConfigureNewEngineLoggerWithCustomCategoryLogger()
    {
        // 1. Application code configures the factory's EngineLogger to use a custom category logger

        ProtonLogger engineLogger = new ProtonLogger(_customCategoryLogger);
        _engineFactory.setEngineLogger(engineLogger);

        // 2. Application code invokes Engine methods, causing logging to occur
        // (for convenience, this test just directly invokes the logger)

        Connection connection = _engineFactory.createConnection();
        assertSame(engineLogger, connection.getEngineLogger());
        engineLogger.outgoingBytes(0, new Open(), ByteBuffer.allocate(0));

        assertThat(_customCategoryLogger._message, containsString("Open"));
        assertThat(_customCategoryLogger._category, CoreMatchers.notNullValue());
    }

    @Test
    public void testConfigureSystemWideDefaultCategoryLogger()
    {
        ProtonLogger.setDefaultCategoryLogger(_customCategoryLogger);
        Connection connection = _engineFactory.createConnection();
        CategoryAwareProtonLogger engineLogger = (CategoryAwareProtonLogger) connection.getEngineLogger();
        assertSame(_customCategoryLogger, engineLogger.getCategoryLogger());
    }

    @Test
    public void testConfigureEngineLoggerPerConnection()
    {
        Connection connection = _engineFactory.createConnection();
        ProtonLogger loggerForThisConnection = new ProtonLogger();
        connection.setEngineLogger(loggerForThisConnection);
        assertSame(loggerForThisConnection, connection.getEngineLogger());
    }

    private class RememberingLogger implements ProtonCategoryLogger
    {
        private String _category;
        private String _message;

        @Override
        public void log(String category, ProtonLogLevel level, String message)
        {
            _category = category;
            _message = message;
        }

        @Override
        public boolean isEnabled(String category, ProtonLogLevel level)
        {
            return true;
        }
    }
}
