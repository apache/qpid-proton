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

import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.apache.qpid.proton.apireconciliation.reportwriter.AnnotationAccessor;

public class Joiner
{
    private final AnnotationAccessor _annotationAccessor;

    public Joiner(AnnotationAccessor annotationAccessor)
    {
        _annotationAccessor = annotationAccessor;
    }

    /**
     * Does an outer join of the supplied C functions with those named by the
     * annotations on the Java methods.
     */
    public ReconciliationReport join(List<String> protonCFunctions, Set<Method> javaMethods)
    {
        ReconciliationReport report = new ReconciliationReport();

        Map<String, Method> cFunctionToJavaMethodMap = createOneToOneMappingBetweenCFunctionNameAndJavaMethodMap(javaMethods);

        Set<Method> unannotatedMethods = new HashSet<Method>(javaMethods);
        unannotatedMethods.removeAll(cFunctionToJavaMethodMap.values());

        for (Method unannotatedMethod : unannotatedMethods)
        {
            report.addRow(null, unannotatedMethod);
        }

        for (String protonCFunction : protonCFunctions)
        {
            Method javaMethod = cFunctionToJavaMethodMap.remove(protonCFunction);
            report.addRow(protonCFunction, javaMethod);
        }

        // add anything remaining in annotatedNameToMethod to report as Java methods with an unknown annotation
        for (Method method : cFunctionToJavaMethodMap.values())
        {
            report.addRow(null, method);
        }

        return report;
    }

    private Map<String, Method> createOneToOneMappingBetweenCFunctionNameAndJavaMethodMap(Set<Method> javaMethods)
    {
        Map<String, Method> annotatedNameToMethod = new HashMap<String, Method>();
        Set<String> functionsWithDuplicateJavaMappings = new HashSet<String>();

        for (Method method : javaMethods)
        {
            String functionName = _annotationAccessor.getAnnotationValue(method);
            if (functionName != null)
            {
                if (annotatedNameToMethod.containsKey(functionName))
                {
                    functionsWithDuplicateJavaMappings.add(functionName);
                }
                annotatedNameToMethod.put(functionName, method);
            }
        }

        // Any functions that had duplicate java method names are removed.
        for (String functionName : functionsWithDuplicateJavaMappings)
        {
            annotatedNameToMethod.remove(functionName);
        }

        return annotatedNameToMethod;
    }

}
