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
 *
 */
package org.apache.qpid.proton.apireconciliation.reportwriter;

import static org.junit.Assert.*;

import java.lang.reflect.Method;

import org.apache.qpid.proton.apireconciliation.TestAnnotation;
import org.junit.Before;
import org.junit.Test;

public class AnnotationAccessorTest
{
    private static final String ANNOTATION_VALUE_1 = "value1";
    private static final String ANNOTATED_METHOD_NAME = "annotatedMethod";
    private static final String UNANNOTATED_METHOD_NAME = "unannotatedMethod";

    private Method _annotatedMethod;
    private Method _unannotatedMethod;

    private String _annotationClassName;

    private AnnotationAccessor _annotationAccessor;

    @Before
    public void setUp() throws Exception
    {
        _annotatedMethod = getClass().getMethod(ANNOTATED_METHOD_NAME);
        _unannotatedMethod = getClass().getMethod(UNANNOTATED_METHOD_NAME);
        _annotationClassName = TestAnnotation.class.getName();
        _annotationAccessor = new AnnotationAccessor(_annotationClassName);
    }

    @Test
    public void testGetAnnotationValue()
    {
        assertEquals(ANNOTATION_VALUE_1, _annotationAccessor.getAnnotationValue(_annotatedMethod));
    }

    @Test
    public void testGetAnnotationValueWithoutAnnotationReturnsNull()
    {
        assertNull(_annotationAccessor.getAnnotationValue(_unannotatedMethod));
    }

    @TestAnnotation(ANNOTATION_VALUE_1)
    public void annotatedMethod()
    {
    }

    public void unannotatedMethod()
    {
    }
}
