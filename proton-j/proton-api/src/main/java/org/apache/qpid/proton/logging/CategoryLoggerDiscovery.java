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

/**
 * Returns a {@link ProtonCategoryLogger} based on the built-in default, a statically overridden one,
 * or an instance specific one (in that order).
 *
 * Thread-safe.
 */
class CategoryLoggerDiscovery
{
    public static final String PROTON_DEFAULT_CATEGORY_LOGGER_PROP = "proton.default_category_logger";
    public static final String PROTON_CATEGORY_LOGGER_JUL = "JUL";
    public static final String PROTON_CATEGORY_LOGGER_SLF4J = "SLF4J";
    public static final String PROTON_CATEGORY_LOGGER_STDOUT = "STDOUT";

    private static final ProtonCategoryLogger DEFAULT_LOGGER;
    static
    {
        String loggerType = System.getProperty(PROTON_DEFAULT_CATEGORY_LOGGER_PROP, PROTON_CATEGORY_LOGGER_STDOUT);
        if(PROTON_CATEGORY_LOGGER_STDOUT.equals(loggerType))
        {
            DEFAULT_LOGGER = new StdOutCategoryLogger();
        }
        else if(PROTON_CATEGORY_LOGGER_JUL.equals(loggerType))
        {
            DEFAULT_LOGGER = createLogger("org.apache.qpid.proton.logging.JULCategoryLogger");
        }
        else if(PROTON_CATEGORY_LOGGER_SLF4J.equals(loggerType))
        {
            DEFAULT_LOGGER = createLogger("org.apache.qpid.proton.logging.SLF4JCategoryLogger");
        }
        else
        {
            DEFAULT_LOGGER = createLogger(loggerType);
        }
    }

    private static ProtonCategoryLogger createLogger(String loggerClass)
    {
        try
        {
            Class<?> clazz = Class.forName(loggerClass);
            if(!ProtonCategoryLogger.class.isAssignableFrom(clazz))
            {
                throw new IllegalArgumentException("Provided class name must be a " +
                                                   ProtonCategoryLogger.class.getName() + ": " + loggerClass);
            }

            Object obj = clazz.newInstance();
            return (ProtonCategoryLogger) obj;
        }
        catch (ClassNotFoundException e)
        {
            throw new RuntimeException(e);
        }
        catch (InstantiationException e)
        {
            throw new RuntimeException(e);
        }
        catch (IllegalAccessException e)
        {
            throw new RuntimeException(e);
        }
    }

    private static volatile ProtonCategoryLogger _overriddenDefaultLogger = null;

    private volatile ProtonCategoryLogger _logger = null;

    static void setDefault(ProtonCategoryLogger defaultDelegate)
    {
        _overriddenDefaultLogger = defaultDelegate;
    }

    void setLogger(ProtonCategoryLogger logger)
    {
        _logger = logger;
    }

    static ProtonCategoryLogger getEffectiveDefaultLogger()
    {
        if(_overriddenDefaultLogger != null)
        {
            return _overriddenDefaultLogger;
        }
        else
        {
            return DEFAULT_LOGGER;
        }
    }

    ProtonCategoryLogger getEffectiveLogger()
    {
        if(_logger != null)
        {
            return _logger;
        }
        else
        {
            return getEffectiveDefaultLogger();
        }
    }
}
