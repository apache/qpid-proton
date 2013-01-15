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

import static java.lang.String.format;
import static org.apache.commons.lang.StringUtils.defaultString;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import org.apache.commons.io.FileUtils;
import org.apache.qpid.proton.apireconciliation.ReconciliationReport;
import org.apache.qpid.proton.apireconciliation.ReportRow;

public class ReconciliationReportWriter
{
    private static final String ROW_FORMAT="%s,%s,%s";
    private static final String REPORT_TITLE = format(ROW_FORMAT, "C function","Java Method","Java Annotation");
    private final AnnotationAccessor _annotationAccessor;

    public ReconciliationReportWriter(AnnotationAccessor annotationAccessor)
    {
        _annotationAccessor = annotationAccessor;
    }

    public void write(String outputFile, ReconciliationReport report) throws IOException
    {
        File output = new File(outputFile);
        List<String> reportLines = new ArrayList<String>();

        reportLines.add(REPORT_TITLE);

        Iterator<ReportRow> itr = report.rowIterator();
        while (itr.hasNext())
        {
            ReportRow row = itr.next();
            Method javaMethod = row.getJavaMethod();
            String cFunction = defaultString(row.getCFunction());

            String fullyQualifiedMethodName = "";
            String annotationCFunction = "";
            if (javaMethod != null)
            {
                fullyQualifiedMethodName = createFullyQualifiedJavaMethodName(javaMethod);
                annotationCFunction = defaultString(_annotationAccessor.getAnnotationValue(javaMethod));
            }
            reportLines.add(format(ROW_FORMAT, cFunction, fullyQualifiedMethodName, annotationCFunction));
        }

        FileUtils.writeLines(output, reportLines);
    }

    private String createFullyQualifiedJavaMethodName(Method javaMethod)
    {
        return javaMethod.getDeclaringClass().getName() +  "#" + javaMethod.getName();
    }

}
