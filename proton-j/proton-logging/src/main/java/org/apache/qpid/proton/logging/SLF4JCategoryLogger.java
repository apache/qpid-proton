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

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class SLF4JCategoryLogger implements ProtonCategoryLogger
{
    @Override
    public boolean isEnabled(String category, ProtonLogLevel level)
    {
        Logger logger = LoggerFactory.getLogger(category);

        switch(level)
        {
            case TRACE:
                return logger.isTraceEnabled();
            case DEBUG:
                return logger.isDebugEnabled();
            case INFO:
                return logger.isInfoEnabled();
            case WARN:
                return logger.isWarnEnabled();
            case ERROR:
                return logger.isErrorEnabled();
            default:
                throw new RuntimeException("Unsupported log level: " + level);
        }
    }

    @Override
    public void log(String category, ProtonLogLevel level, String message)
    {
        Logger logger = LoggerFactory.getLogger(category);

        switch(level)
        {
            case TRACE:
                logger.trace(message);
                break;
            case DEBUG:
                logger.debug(message);
                break;
            case INFO:
                logger.info(message);
                break;
            case WARN:
                logger.warn(message);
                break;
            case ERROR:
                logger.error(message);
                break;
            default:
                throw new RuntimeException("Unsupported log level: " + level);
        }
    }
}
