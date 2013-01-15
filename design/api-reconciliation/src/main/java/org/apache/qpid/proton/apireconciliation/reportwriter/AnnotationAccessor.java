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

import java.lang.annotation.Annotation;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class AnnotationAccessor
{
    private static final String VALUE_METHOD = "value";
    private final Class<Annotation> _annotationClass;
    private final Method _functionNameMethod;

    @SuppressWarnings("unchecked")
    public AnnotationAccessor(String annotationClassName)
    {
        try
        {
            _annotationClass = (Class<Annotation>) Class.forName(annotationClassName);
        }
        catch (ClassNotFoundException e)
        {
            throw new IllegalArgumentException("Couldn't find annotation class " + annotationClassName, e);
        }

        try
        {
            _functionNameMethod = _annotationClass.getMethod(VALUE_METHOD);
        }
        catch (SecurityException e)
        {
            throw new IllegalArgumentException("Couldn't find method " + VALUE_METHOD + " on annotation " + _annotationClass, e);
        }
        catch (NoSuchMethodException e)
        {
            throw new IllegalArgumentException("Couldn't find method " + VALUE_METHOD + " on annotation " + _annotationClass, e);
        }
    }

    public String getAnnotationValue(Method javaMethod)
    {
        Annotation annotation = javaMethod.getAnnotation(_annotationClass);
        if (javaMethod != null && annotation != null)
        {
            return getProtonCFunctionName(annotation);
        }
        else
        {
            return null;
        }
    }

    private String getProtonCFunctionName(Annotation annotation)
    {
        try
        {
            return String.valueOf(_functionNameMethod.invoke(annotation));
        }
        catch (IllegalArgumentException e)
        {
            throw new RuntimeException("Couldn't invoke method " + _functionNameMethod + " on annotation " + annotation, e);
        }
        catch (IllegalAccessException e)
        {
            throw new RuntimeException("Couldn't invoke method " + _functionNameMethod + " on annotation " + annotation, e);
        }
        catch (InvocationTargetException e)
        {
            throw new RuntimeException("Couldn't invoke method " + _functionNameMethod + " on annotation " + annotation, e);
        }
    }
}
