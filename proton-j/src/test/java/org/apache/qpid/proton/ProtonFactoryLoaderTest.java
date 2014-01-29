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
package org.apache.qpid.proton;

import static org.apache.qpid.proton.ProtonFactory.ImplementationType.ANY;
import static org.apache.qpid.proton.ProtonFactory.ImplementationType.PROTON_C;
import static org.apache.qpid.proton.ProtonFactory.ImplementationType.PROTON_J;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import org.apache.qpid.proton.ProtonFactory.ImplementationType;
import org.apache.qpid.proton.factoryloadertesting.DummyProtonFactory;
import org.junit.Test;

public class ProtonFactoryLoaderTest
{
    private String _previousImplementationType;

    @Test
    public void testLoadFactoryForAnyImplementationType()
    {
        ImplementationType implementationType = ANY;

        ProtonFactoryLoader<DummyProtonFactory> factoryLoader =
                new ProtonFactoryLoader<DummyProtonFactory>(DummyProtonFactory.class, implementationType);

        assertNotNull(factoryLoader);
    }

    @Test
    public void testLoadFactoryForProtonJ()
    {
        testImplementationType(PROTON_J);
    }

    @Test
    public void testLoadFactoryForProtonC()
    {
        testImplementationType(PROTON_C);
    }

    private void testImplementationType(ImplementationType implementationType)
    {
        ProtonFactoryLoader<DummyProtonFactory> factoryLoader =
                new ProtonFactoryLoader<DummyProtonFactory>(DummyProtonFactory.class, implementationType);

        assertEquals(implementationType, factoryLoader.loadFactory().getImplementationType());
    }

    @Test
    public void testLoadFactoryUsingProtonCImplementationTypeFromSystemProperty()
    {
        testLoadFactoryUsingImplementationTypeFromSystemProperty(PROTON_C.name());
    }

    @Test
    public void testLoadFactoryUsingProtonJImplementationTypeFromSystemProperty()
    {
        testLoadFactoryUsingImplementationTypeFromSystemProperty(PROTON_J.name());
    }

    @Test
    public void testLoadFactoryUsingDefaultImplementationType()
    {
        testLoadFactoryUsingImplementationTypeFromSystemProperty(null);
    }

    private void testLoadFactoryUsingImplementationTypeFromSystemProperty(String implementationTypeName)
    {
        try
        {
            setImplementationTypeSystemProperty(implementationTypeName);
            ProtonFactoryLoader<DummyProtonFactory> factoryLoader = new ProtonFactoryLoader<DummyProtonFactory>(DummyProtonFactory.class);
            DummyProtonFactory factory = factoryLoader.loadFactory();

            assertNotNull(factory);

            if(implementationTypeName != null)
            {
                assertEquals(
                        ImplementationType.valueOf(implementationTypeName),
                        factory.getImplementationType());
            }
        }
        finally
        {
            resetImplementationTypeSystemProperty();
        }
    }

    private void setImplementationTypeSystemProperty(String implementationTypeName)
    {
        _previousImplementationType = System.getProperty(ProtonFactoryLoader.IMPLEMENTATION_TYPE_PROPERTY);
        setOrClearSystemProperty(implementationTypeName);
    }

    private void resetImplementationTypeSystemProperty()
    {
        setOrClearSystemProperty(_previousImplementationType);
    }

    private void setOrClearSystemProperty(String propertyValue)
    {
        if(propertyValue == null)
        {
            System.clearProperty(ProtonFactoryLoader.IMPLEMENTATION_TYPE_PROPERTY);
        }
        else
        {
            System.setProperty(ProtonFactoryLoader.IMPLEMENTATION_TYPE_PROPERTY, propertyValue);
        }
    }
}
