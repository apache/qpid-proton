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

import java.util.Date;
import java.util.concurrent.ConcurrentHashMap;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Logs the enabled categories to standard out.
 *
 * Categories are enabled by setting a regular expression specifying the categories
 * via system property {@value #PROTON_ENABLED_CATEGORIES_PROP} or, if absent,
 * environment variable {@value #PROTON_ENABLED_CATEGORIES_ENV}.
 */
public class StdOutCategoryLogger implements ProtonCategoryLogger
{
    public static final String PROTON_ENABLED_CATEGORIES_PROP = "proton.logging.categories";
    public static final String PROTON_ENABLED_CATEGORIES_ENV = "PN_LOGGING_CATEGORIES";

    private ConcurrentHashMap<String, Boolean> _categoryEnabledStatuses = new ConcurrentHashMap<String, Boolean>();

    private final Pattern _enabledCategoriesPattern;

    public StdOutCategoryLogger()
    {
        String enabledCategoriesEnv = System.getenv(PROTON_ENABLED_CATEGORIES_ENV);
        String enabledCategoriesString = System.getProperty(PROTON_ENABLED_CATEGORIES_PROP, enabledCategoriesEnv);
        if(enabledCategoriesString == null || enabledCategoriesString.isEmpty())
        {
            _enabledCategoriesPattern = null;
        }
        else
        {
            _enabledCategoriesPattern = Pattern.compile(enabledCategoriesString);
        }
    }

    /**
     * Returns whether the supplied category is enabled.
     */
    @Override
    public boolean isEnabled(String category, ProtonLogLevel level)
    {
        if(_enabledCategoriesPattern == null)
        {
            return false;
        }
        else
        {
            Boolean enabledStatus = _categoryEnabledStatuses.get(category);
            if(enabledStatus == null)
            {
                Matcher matcher = _enabledCategoriesPattern.matcher(category);
                boolean matches = matcher.matches();
                _categoryEnabledStatuses.putIfAbsent(category, matches);
                return matches;
            }
            else
            {
                return enabledStatus;
            }
        }
    }

    /**
     * Logs the message if this category is enabled.
     */
    @Override
    public void log(String category, ProtonLogLevel level, String message)
    {
        if(isEnabled(category, level))
        {
            System.out.println(String.format("%1$tF %1$tT.%tL [%s] [%s] %s",
                                             new Date(), level, category, message));
        }
    }
}
