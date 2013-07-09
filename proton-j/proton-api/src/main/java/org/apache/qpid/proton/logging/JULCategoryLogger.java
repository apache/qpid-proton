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

import java.util.HashMap;
import java.util.Map;
import java.util.logging.Level;
import java.util.logging.Logger;


/**
 * Uses java.util.logging
 */
public class JULCategoryLogger implements ProtonCategoryLogger
{
    private static final Map<ProtonLogLevel, Level> _protonLevelsToJavaLevels = new HashMap<ProtonLogLevel, Level>();
    static
    {
        _protonLevelsToJavaLevels.put(ProtonLogLevel.TRACE, Level.FINER);
        _protonLevelsToJavaLevels.put(ProtonLogLevel.DEBUG, Level.FINE);
        _protonLevelsToJavaLevels.put(ProtonLogLevel.INFO, Level.INFO);
        _protonLevelsToJavaLevels.put(ProtonLogLevel.WARN, Level.WARNING);
        _protonLevelsToJavaLevels.put(ProtonLogLevel.ERROR, Level.SEVERE);
    }

    @Override
    public boolean isEnabled(String category, ProtonLogLevel level)
    {
        Level julLevel = _protonLevelsToJavaLevels.get(level);
        return Logger.getLogger(category).isLoggable(julLevel);
    }

    @Override
    public void log(String category, ProtonLogLevel level, String message)
    {
        Level julLevel = _protonLevelsToJavaLevels.get(level);
        Logger.getLogger(category).log(julLevel, message);
    }
}
