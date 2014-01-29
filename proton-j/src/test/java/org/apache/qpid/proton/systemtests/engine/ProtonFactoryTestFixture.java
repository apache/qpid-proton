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
package org.apache.qpid.proton.systemtests.engine;

import static org.apache.qpid.proton.ProtonFactory.ImplementationType.PROTON_C;
import static org.apache.qpid.proton.ProtonFactory.ImplementationType.PROTON_J;

import org.apache.qpid.proton.ProtonFactoryLoader;
import org.apache.qpid.proton.engine.EngineFactory;

public class ProtonFactoryTestFixture
{
    private final EngineFactory _engineFactory = new ProtonFactoryLoader<EngineFactory>(EngineFactory.class).loadFactory();

    public static boolean isProtonC(EngineFactory engineFactory)
    {
        return engineFactory.getImplementationType() == PROTON_C;
    }

    public static boolean isProtonJ(EngineFactory engineFactory)
    {
        return engineFactory.getImplementationType() == PROTON_J;
    }

    /**
     * TODO support different implementations for factory1 and factory2
     */
    public EngineFactory getFactory1()
    {
        return _engineFactory;
    }

    public EngineFactory getFactory2()
    {
        return _engineFactory;
    }

}
