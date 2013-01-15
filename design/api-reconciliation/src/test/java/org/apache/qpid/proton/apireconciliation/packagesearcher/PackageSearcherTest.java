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

import static org.junit.Assert.assertEquals;

import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

import org.apache.qpid.proton.apireconciliation.packagesearcher.testdata.manyimpls.Impl1;
import org.apache.qpid.proton.apireconciliation.packagesearcher.testdata.manyimpls.Impl2;
import org.apache.qpid.proton.apireconciliation.packagesearcher.testdata.manyimpls.InterfaceWithManyImpls;
import org.apache.qpid.proton.apireconciliation.packagesearcher.testdata.noimpl.InterfaceWithoutImpl;
import org.apache.qpid.proton.apireconciliation.packagesearcher.testdata.tree.ImplAtTreeTop;
import org.apache.qpid.proton.apireconciliation.packagesearcher.testdata.tree.InterfaceAtTreeTop;
import org.apache.qpid.proton.apireconciliation.packagesearcher.testdata.tree.leaf.ImplAtLeaf;
import org.junit.Test;

public class PackageSearcherTest
{
    private PackageSearcher _packageSearcher = new PackageSearcher();

    @Test
    public void testFindDescendsPackageTree() throws Exception
    {
        String testDataPackage = InterfaceAtTreeTop.class.getPackage().getName();
        Set<Method> actualMethods = _packageSearcher.findMethods(testDataPackage);
        assertEquals(2, actualMethods.size());

        Set<Method> expectedMethods = new HashSet<Method>(Arrays.asList(
                ImplAtTreeTop.class.getMethod("method"),
                ImplAtLeaf.class.getMethod("method")));

        assertEquals(expectedMethods, actualMethods);
    }

    @Test
    public void testZeroImplenentationsOfInterface() throws Exception
    {
        String testDataPackage = InterfaceWithoutImpl.class.getPackage().getName();

        Method expectedMethod = InterfaceWithoutImpl.class.getMethod("method");

        Set<Method> actualMethods = _packageSearcher.findMethods(testDataPackage);
        assertEquals(1, actualMethods.size());

        Method actualMethod = actualMethods.iterator().next();
        assertEquals(expectedMethod, actualMethod);
    }

    @Test
    public void testManyImplenentationsOfInterface() throws Exception
    {
        String testDataPackage = InterfaceWithManyImpls.class.getPackage().getName();

        Set<Method> actualMethods = _packageSearcher.findMethods(testDataPackage);
        assertEquals(2, actualMethods.size());

        String methodName = "method";
        Set<Method> expectedMethods = new HashSet<Method>(Arrays.asList(
                Impl1.class.getMethod(methodName),
                Impl2.class.getMethod(methodName)));

        assertEquals(expectedMethods, actualMethods);
    }
}
