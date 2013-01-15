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
package org.apache.qpid.proton.apireconciliation.packagesearcher;

import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.logging.Logger;

import org.reflections.Reflections;
import org.reflections.scanners.SubTypesScanner;

public class PackageSearcher
{
    private final static Logger LOGGER = Logger.getLogger(PackageSearcher.class.getName());

    public Set<Method> findMethods(String packageName)
    {
        Reflections reflections = new Reflections(packageName, new SubTypesScanner(false));

        Set<Class<?>> allInterfaces = getAllApiInterfaces(reflections);

        Set<Method> allImplMethods = new HashSet<Method>();
        for (Class<?> apiInterface : allInterfaces)
        {
            List<Method> apiMethodList = Arrays.asList(apiInterface.getMethods());
            Set<?> impls = reflections.getSubTypesOf(apiInterface);
            if (impls.size() == 0)
            {
                // In the case where there are no implementations of apiInterface, we add the methods of
                // apiInterface so they appear on the final report.
                for (Method apiMethod : apiMethodList)
                {
                    allImplMethods.add(apiMethod);
                }
            }
            else
            {
                for (Object implementingClassObj : impls)
                {
                    Class implementingClass = (Class) implementingClassObj;
                    LOGGER.fine("Found implementation " + implementingClass.getName() + " for " + apiInterface.getName());

                    for (Method apiMethod : apiMethodList)
                    {
                        Method implMethod = findImplMethodOfApiMethod(apiMethod, implementingClass);
                        allImplMethods.add(implMethod);
                    }
                }
            }
        }
        return allImplMethods;
    }

    private Method findImplMethodOfApiMethod(Method apiMethod, Class<?> impl)
    {
        try
        {
            Method implMethod = impl.getMethod(apiMethod.getName(), apiMethod.getParameterTypes());
            return implMethod;
        }
        catch (Exception e)
        {
            // Should not happen
            throw new IllegalStateException("Could not find implementation of method " + apiMethod
                    + " on the impl. " + impl, e);
        }
    }

    @SuppressWarnings("rawtypes")
    private Set<Class<?>> getAllApiInterfaces(Reflections reflections)
    {
        Set<Class<?>> classes = reflections.getSubTypesOf(Object.class);
        Set<Class<?>> interfaces = new HashSet<Class<?>>();

        for (Class clazz : classes)
        {
            if(clazz.isInterface())
            {
                interfaces.add(clazz);
            }
        }

        return interfaces;
    }

}
