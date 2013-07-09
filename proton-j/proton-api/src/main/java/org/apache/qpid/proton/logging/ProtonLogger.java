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
package org.apache.qpid.proton.logging;

import java.nio.ByteBuffer;

import org.apache.qpid.proton.amqp.transport.FrameBody;
import org.apache.qpid.proton.engine.EngineLogger;

/**
 * An Engine Logger that delegates to a {@link ProtonCategoryLogger}.
 *
 * Thread-safe.
 *
 * The default {@link ProtonCategoryLogger} can be controlled using the system property
 * {@value org.apache.qpid.proton.logging.CategoryLoggerDiscovery#PROTON_DEFAULT_CATEGORY_LOGGER_PROP}, specifying the class name
 * of a particular implementation to use. Additionally, convenience values can be used for the following:
 *
 * <ul>
 *  <li>{@value org.apache.qpid.proton.logging.CategoryLoggerDiscovery#PROTON_CATEGORY_LOGGER_JUL} : java.util.logging</li>
 *  <li>{@value org.apache.qpid.proton.logging.CategoryLoggerDiscovery#PROTON_CATEGORY_LOGGER_SLF4J} : SLF4J</li>
 *  <li>{@value org.apache.qpid.proton.logging.CategoryLoggerDiscovery#PROTON_CATEGORY_LOGGER_STDOUT} : StdOut</li>
 * </ul>
 */
public class ProtonLogger implements EngineLogger, CategoryAwareProtonLogger
{
    private static final String BYTES_OUT_CATEGORY = "proton.bytes.out";

    private final CategoryLoggerDiscovery _categoryLoggerDiscovery = new CategoryLoggerDiscovery();

    public static ProtonCategoryLogger getDefaultCategoryLogger()
    {
        return CategoryLoggerDiscovery.getEffectiveDefaultLogger();
    }

    /**
     * Sets the default category logger, or restores it to the built-in default if
     * the supplied logger is null.
     */
    public static void setDefaultCategoryLogger(ProtonCategoryLogger categoryLogger)
    {
        CategoryLoggerDiscovery.setDefault(categoryLogger);
    }

    /**
     * Constructor that uses the default {@link ProtonCategoryLogger}.
     */
    public ProtonLogger()
    {
    }

    public ProtonLogger(ProtonCategoryLogger categoryLogger)
    {
        setCategoryLogger(categoryLogger);
    }

    @Override
    public void outgoingBytes(int channel, FrameBody frameBody, ByteBuffer payload)
    {
        ProtonCategoryLogger logger = _categoryLoggerDiscovery.getEffectiveLogger();
        if(logger.isEnabled(BYTES_OUT_CATEGORY, ProtonLogLevel.TRACE))
        {
            logger.log(
                    BYTES_OUT_CATEGORY,
                    ProtonLogLevel.TRACE,
                    "PHTODO generate outgoingBytes message from " + frameBody);
        }
    }

    @Override
    public ProtonCategoryLogger getCategoryLogger()
    {
        return _categoryLoggerDiscovery.getEffectiveLogger();
    }

    @Override
    public void setCategoryLogger(ProtonCategoryLogger logger)
    {
        _categoryLoggerDiscovery.setLogger(logger);
    }
}
