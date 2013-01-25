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

import java.util.Iterator;
import java.util.ServiceLoader;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 * A thin wrapper around {@link ServiceLoader} intended for loading Proton object factories.
 */
public class ProtonFactoryLoader<C>
{
    private static final Logger LOGGER = Logger.getLogger(ProtonFactoryLoader.class.getName());
    private Class<C> _factoryInterface;

    /**
     * Use this constructor if you intend to explicitly provide factory interface later,
     * i.e. by calling {@link #loadFactory(Class)}. This is useful if you want to use the same
     * ProtonFactoryLoader instance for loading multiple factory types.
     */
    public ProtonFactoryLoader()
    {
    }

    /**
     * @param factoryInterface will be used as the factory interface class in calls to {@link #loadFactory()}.
     */
    public ProtonFactoryLoader(Class<C> factoryInterface)
    {
        _factoryInterface = factoryInterface;
    }

    /**
     * Returns the Proton factory that implements the stored {@link ProtonFactoryLoader#_factoryInterface} class.
     */
    public C loadFactory()
    {
        return loadFactory(_factoryInterface);
    }

    public C loadFactory(Class<C> factoryInterface)
    {
        if(factoryInterface == null)
        {
            throw new IllegalStateException("factoryInterface has not been set.");
        }
        ServiceLoader<C> serviceLoader = ServiceLoader.load(factoryInterface);
        Iterator<C> serviceLoaderIterator = serviceLoader.iterator();
        if(!serviceLoaderIterator.hasNext())
        {
            throw new IllegalStateException("Can't find service loader for " + factoryInterface.getName());
        }
        C factory = serviceLoaderIterator.next();
        if(LOGGER.isLoggable(Level.FINE))
        {
            LOGGER.fine("loadFactory returning " + factory);
        }
        return factory;
    }

}
