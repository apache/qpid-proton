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
package org.apache.qpid.proton.apireconciliation;

import java.util.List;

import org.apache.qpid.proton.apireconciliation.reportwriter.AnnotationAccessor;
import org.apache.qpid.proton.apireconciliation.reportwriter.ReconciliationReportWriter;

public class Main
{

    public static void main(String[] args) throws Exception
    {
        if (args.length != 4)
        {
            System.err.println("Unexpected number of arguments. Usage:");
            System.err.println("    java " + Main.class.getName() + " packageRootName cFunctionFile annotationClassName outputFile");
            Runtime.getRuntime().exit(-1);
        }

        String packageRootName = args[0];
        String cFunctionFile = args[1];
        String annotationClassName = args[2];
        String outputFile = args[3];

        CFunctionNameListReader cFunctionNameListReader = new CFunctionNameListReader();

        AnnotationAccessor annotationAccessor = new AnnotationAccessor(annotationClassName);
        Reconciliation reconciliation = new Reconciliation(annotationAccessor);

        List<String> cFunctionNames = cFunctionNameListReader.readCFunctionNames(cFunctionFile);
        ReconciliationReport report = reconciliation.reconcile(cFunctionNames, packageRootName);

        ReconciliationReportWriter writer = new ReconciliationReportWriter(annotationAccessor);
        writer.write(outputFile, report);
        System.err.println("Written : " + outputFile);
    }

}
