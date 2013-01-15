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
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;

import java.lang.reflect.Method;
import java.util.Collections;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

import org.apache.qpid.proton.apireconciliation.reportwriter.AnnotationAccessor;
import org.junit.Before;
import org.junit.Test;

public class JoinerTest
{
    private static final String C_FUNCTION1 = "cFunction1";
    private static final String C_FUNCTION2 = "cFunction2";
    private Joiner _joiner;
    private Method _method1 = null;
    private Method _method2 = null;
    private Method _methodSharingFunctionNameAnnotationWithMethod2 = null;
    private Method _methodWithoutAnnotation;

    @Before
    public void setUp() throws Exception
    {
        _method1 = getClass().getMethod("javaMethodWithMapping1");
        _method2 = getClass().getMethod("javaMethodWithMapping2");
        _methodSharingFunctionNameAnnotationWithMethod2 = getClass().getMethod("javaMethodSharingFunctionNameAnnotationWithMethod2");
        _methodWithoutAnnotation = getClass().getMethod("javaMethodWithoutAnnotation");

        AnnotationAccessor annotationAccessor = new AnnotationAccessor(TestAnnotation.class.getName());

        _joiner = new Joiner(annotationAccessor);
    }

    @Test
    public void testSingleRowReport() throws Exception
    {
        List<String> protonCFunctions = asList(C_FUNCTION1);
        Set<Method> javaMethods = new HashSet<Method>(asList(_method1));

        ReconciliationReport reconciliationReport = _joiner.join(protonCFunctions, javaMethods);
        assertSingleRowEquals(reconciliationReport, C_FUNCTION1, _method1);
    }

    @Test
    public void testCFunctionWithoutCorrespondingAnnotatedJavaMethod() throws Exception
    {
        List<String> protonCFunctions = asList("functionX");
        Set<Method> javaMethods = Collections.emptySet();

        ReconciliationReport reconciliationReport = _joiner.join(protonCFunctions, javaMethods);
        assertSingleRowEquals(reconciliationReport, "functionX", null);
    }

    @Test
    public void testJavaMethodAnnotatedWithUnknownCFunctionName() throws Exception
    {
        List<String> protonCFunctions = Collections.emptyList();
        Set<Method> javaMethods = new HashSet<Method>(asList(_method1));

        ReconciliationReport reconciliationReport = _joiner.join(protonCFunctions, javaMethods);
        assertSingleRowEquals(reconciliationReport, null, _method1);
    }

    @Test
    public void testJavaMethodWithoutAnnotation() throws Exception
    {
        List<String> protonCFunctions = Collections.emptyList();
        Set<Method> javaMethods = new HashSet<Method>(asList(_methodWithoutAnnotation));

        ReconciliationReport reconciliationReport = _joiner.join(protonCFunctions, javaMethods);
        assertSingleRowEquals(reconciliationReport, null, _methodWithoutAnnotation);
    }

    @Test
    public void testJavaMethodsWithAnnotationToSameFunction() throws Exception
    {
        List<String> protonCFunctions = asList(C_FUNCTION2);
        Set<Method> javaMethods = new HashSet<Method>(asList(_method2, _methodSharingFunctionNameAnnotationWithMethod2));

        ReconciliationReport reconciliationReport = _joiner.join(protonCFunctions, javaMethods);
        Set<ReportRow> rowSet = TestUtils.getReportRowsFrom(reconciliationReport);

        Set<ReportRow> expectedRowSet = new HashSet<ReportRow>(asList(
                new ReportRow(C_FUNCTION2, null),
                new ReportRow(null, _method2),
                new ReportRow(null, _methodSharingFunctionNameAnnotationWithMethod2)));

        assertEquals(expectedRowSet, rowSet);
    }

    @Test
    public void testMultipleRowReport() throws Exception
    {
        List<String> protonCFunctions = asList(C_FUNCTION1, C_FUNCTION2);
        Set<Method> javaMethods = new HashSet<Method>(asList(_method1, _method2));

        ReconciliationReport reconciliationReport = _joiner.join(protonCFunctions, javaMethods);

        Set<ReportRow> rowSet = TestUtils.getReportRowsFrom(reconciliationReport);

        Set<ReportRow> expectedRowSet = new HashSet<ReportRow>(asList(
                new ReportRow(C_FUNCTION1, _method1),
                new ReportRow(C_FUNCTION2, _method2)));

        assertEquals(expectedRowSet,rowSet);
    }

    private void assertSingleRowEquals(ReconciliationReport reconciliationReport, String expectedCFunctionName, Method expectedJavaMethod)
    {
        Iterator<ReportRow> rowIterator = reconciliationReport.rowIterator();
        ReportRow row = rowIterator.next();
        assertReportRowEquals(row, expectedCFunctionName, expectedJavaMethod);

        assertFalse(rowIterator.hasNext());
    }

    private void assertReportRowEquals(ReportRow row, String expectedCFunctionName, Method expectedMethod)
    {
        assertEquals(expectedCFunctionName, row.getCFunction());
        assertEquals(expectedMethod, row.getJavaMethod());
    }

    @TestAnnotation(C_FUNCTION1)
    public void javaMethodWithMapping1()
    {
    }

    @TestAnnotation(C_FUNCTION2)
    public void javaMethodWithMapping2()
    {
    }

    @TestAnnotation(C_FUNCTION2)
    public void javaMethodSharingFunctionNameAnnotationWithMethod2()
    {
    }

    public void javaMethodWithoutAnnotation()
    {
    }
}
