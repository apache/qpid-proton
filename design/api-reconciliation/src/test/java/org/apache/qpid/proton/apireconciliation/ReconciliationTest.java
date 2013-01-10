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

import static java.util.Arrays.asList;
import static org.junit.Assert.*;

import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import org.apache.qpid.proton.apireconciliation.testdata.InterfaceA;
import org.apache.qpid.proton.apireconciliation.testdata.InterfaceB;
import org.apache.qpid.proton.apireconciliation.testdata.subpackage.InterfaceInSubPackage;
import org.junit.Test;

public class ReconciliationTest
{
    private static final List<String> LIST_OF_C_FUNCTIONS = Arrays.asList(InterfaceA.cFunction1, InterfaceInSubPackage.cFunction);
    private static final String TEST_PACKAGE_TREE_ROOT = "org.apache.qpid.proton.apireconciliation.testdata";

    @Test
    public void test() throws Exception
    {
        Method method = InterfaceA.class.getMethod(InterfaceA.methodA1);
        Method unmappedMethod = InterfaceB.class.getMethod(InterfaceB.methodWithoutAnnotation);
        Method methodWithinSubpackage = InterfaceInSubPackage.class.getMethod(InterfaceInSubPackage.methodWithinSubpackage);

        Reconciliation reconciliation = new Reconciliation();
        ReconciliationReport report = reconciliation.reconcile(LIST_OF_C_FUNCTIONS, TEST_PACKAGE_TREE_ROOT);
        Set<ReportRow> rowSet = TestUtils.getReportRowsFrom(report);

        Set<ReportRow> expectedRowSet = new HashSet<ReportRow>(asList(
                new ReportRow(InterfaceA.cFunction1, method),
                new ReportRow(InterfaceInSubPackage.cFunction, methodWithinSubpackage),
                new ReportRow(null, unmappedMethod)));

        assertEquals(expectedRowSet,rowSet);
    }

}
