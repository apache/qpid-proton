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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;

import org.apache.qpid.proton.test.ProtonTestCase;
import org.junit.Test;

public class StdOutCategoryLoggerTest extends ProtonTestCase
{
    @Test
    public void testIsEnabled_returnsFalseByDefault()
    {
        StdOutCategoryLogger logger = new StdOutCategoryLogger();
        assertFalse(logger.isEnabled("proton.category1", ProtonLogLevel.TRACE));
    }

    @Test
    public void testIsEnabledForExactMatch_returnsTrue()
    {
        setTestSystemProperty(StdOutCategoryLogger.PROTON_ENABLED_CATEGORIES_PROP, "proton.category1");
        StdOutCategoryLogger logger = new StdOutCategoryLogger();
        assertWhetherEnabled(logger, "proton.category1", true);
        assertWhetherEnabled(logger, "notproton.category1", false);
    }

    @Test
    public void testIsEnabledForRegexMatch()
    {
        setTestSystemProperty(StdOutCategoryLogger.PROTON_ENABLED_CATEGORIES_PROP, "proton.*");
        StdOutCategoryLogger logger = new StdOutCategoryLogger();

        assertWhetherEnabled(logger, "proton", true);
        assertWhetherEnabled(logger, "proton", true); // ask again to exercise the logger's memoizing code path
        assertWhetherEnabled(logger, "proton.category1", true);
        assertWhetherEnabled(logger, "notproton", false);
    }

    @Test
    public void testIsEnabledForMultiCategoryRegex()
    {
        setTestSystemProperty(StdOutCategoryLogger.PROTON_ENABLED_CATEGORIES_PROP, "(category1|category2)");
        StdOutCategoryLogger logger = new StdOutCategoryLogger();

        assertWhetherEnabled(logger, "category1", true);
        assertWhetherEnabled(logger, "category2", true);
        assertWhetherEnabled(logger, "category3", false);
    }

    private void assertWhetherEnabled(StdOutCategoryLogger logger, String category, boolean isEnabled)
    {
        assertEquals(isEnabled, logger.isEnabled(category, ProtonLogLevel.TRACE));
    }
}
