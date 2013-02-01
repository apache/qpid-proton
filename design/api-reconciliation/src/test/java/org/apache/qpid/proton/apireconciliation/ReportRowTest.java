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

import static org.junit.Assert.*;

import java.lang.reflect.Method;

import org.junit.Before;
import org.junit.Test;

public class ReportRowTest
{

    private Method _javaMethod1;
    private Method _javaMethod2;

    @Before
    public void setUp() throws Exception
    {
        _javaMethod1 = getClass().getMethod("javaMethod1");
        _javaMethod2 = getClass().getMethod("javaMethod2");
    }

    @Test
    public void testSames() throws Exception
    {

        ReportRow reportRow = new ReportRow("cfunction", _javaMethod1);
        Object other = new Object();

        assertTrue(reportRow.equals(reportRow));
        assertFalse(reportRow.equals(other));
    }

    @Test
    public void testEquals() throws Exception
    {

        assertTrue(new ReportRow("cfunction", _javaMethod1).equals(new ReportRow("cfunction", _javaMethod1)));

        assertFalse(new ReportRow("cfunction", _javaMethod1).equals(new ReportRow("cfunction2", _javaMethod1)));
        assertFalse(new ReportRow("cfunction2", _javaMethod1).equals(new ReportRow("cfunction2", _javaMethod2)));

        assertFalse(new ReportRow("cfunction", _javaMethod1).equals(null));

    }

    @Test
    public void testEqualsWithNulls() throws Exception
    {
        assertTrue(new ReportRow("cfunction", null).equals(new ReportRow("cfunction", null)));
        assertTrue(new ReportRow(null, _javaMethod1).equals(new ReportRow(null, _javaMethod1)));

        assertFalse(new ReportRow("cfunction", _javaMethod1).equals(new ReportRow("cfunction", null)));
        assertFalse(new ReportRow("cfunction", _javaMethod1).equals(new ReportRow(null, _javaMethod1)));
    }

    // Used by reflection by test methods
    public void javaMethod1()
    {
    }

    // Used by reflection by test methods
    public void javaMethod2()
    {
    }
}
