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
package org.apache.qpid.proton.apireconciliation;

import static org.junit.Assert.assertEquals;

import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

import org.apache.qpid.proton.ProtonCEquivalent;
import org.apache.qpid.proton.apireconciliation.testdata.InterfaceA;
import org.apache.qpid.proton.apireconciliation.testdata.InterfaceB;
import org.apache.qpid.proton.apireconciliation.testdata.subpackage.InterfaceInSubPackage;
import org.junit.Test;

public class PackageSearcherTest
{
    @Test
    public void testFindMethods() throws Exception
    {
        PackageSearcher packageSearcher = new PackageSearcher();

        Set<Method> actualMethods = packageSearcher.findMethods(InterfaceA.class.getPackage().getName());
        assertEquals(3, actualMethods.size());

        Set<Method> expectedMethods = new HashSet<Method>(Arrays.asList(
                InterfaceA.class.getMethod(InterfaceA.methodA1),
                InterfaceB.class.getMethod(InterfaceB.methodWithoutAnnotation),
                InterfaceInSubPackage.class.getMethod(InterfaceInSubPackage.methodWithinSubpackage)
                ));

        assertEquals(expectedMethods, actualMethods);
    }

    @Test
    public void testFindMethodsRetainsAnnotations() throws Exception
    {
        PackageSearcher packageSearcher = new PackageSearcher();

        Set<Method> actualMethods = packageSearcher.findMethods(InterfaceInSubPackage.class.getPackage().getName());
        assertEquals(1, actualMethods.size());

        Method actualMethod = actualMethods.iterator().next();
        assertEquals("subpackageFunction",
                     actualMethod.getAnnotation(ProtonCEquivalent.class).functionName());
    }
}
