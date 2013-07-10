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

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.mock;

import org.junit.Test;

public class CategoryLoggerDiscoveryTest
{
    private final CategoryLoggerDiscovery _discovery = new CategoryLoggerDiscovery();

    @Test
    public void testDefaultLogger_isStdOutLogger()
    {
        assertThat(_discovery.getEffectiveLogger(), is(StdOutCategoryLogger.class));
    }

    @Test
    public void testSetDefaultDelegateBeforeCreatingLogger()
    {
        ProtonCategoryLogger categoryLogger = mock(ProtonCategoryLogger.class);
        CategoryLoggerDiscovery.setDefault(categoryLogger);

        assertSame(categoryLogger, (new CategoryLoggerDiscovery()).getEffectiveLogger());
    }

    @Test
    public void testSetDefaultDelegateAfterCreatingLogger()
    {
        ProtonCategoryLogger categoryLogger = mock(ProtonCategoryLogger.class);
        CategoryLoggerDiscovery.setDefault(categoryLogger);

        assertSame(categoryLogger, _discovery.getEffectiveLogger());
    }

    @Test
    public void testSetDelegate_takesPrecedenceOverDefault()
    {
        ProtonCategoryLogger delegate = mock(ProtonCategoryLogger.class);
        _discovery.setLogger(delegate);

        CategoryLoggerDiscovery.setDefault(mock(ProtonCategoryLogger.class));

        assertSame(delegate, _discovery.getEffectiveLogger());
    }
}
