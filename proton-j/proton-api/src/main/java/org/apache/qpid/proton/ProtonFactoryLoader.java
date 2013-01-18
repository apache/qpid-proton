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
import java.util.logging.Logger;

public class ProtonFactoryLoader<C>
{
    private static final Logger LOGGER = Logger.getLogger(ProtonFactoryLoader.class.getName());
    
    public C loadFactory(Class<C> factoryInterface)
    {
        ServiceLoader<C> serviceLoader = ServiceLoader.load(factoryInterface);
        Iterator<C> serviceLoaderIterator = serviceLoader.iterator();
        if(!serviceLoaderIterator.hasNext())
        {
            throw new IllegalStateException("Can't find service loader for " + factoryInterface.getName());
        }
        C factory = serviceLoaderIterator.next();
        LOGGER.info("loadFactory returning " + factory);
        return factory;
    }

}
