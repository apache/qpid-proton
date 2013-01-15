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
import java.util.List;
import java.util.Set;

import org.apache.qpid.proton.apireconciliation.packagesearcher.PackageSearcher;
import org.apache.qpid.proton.apireconciliation.reportwriter.AnnotationAccessor;

public class Reconciliation
{
    private final PackageSearcher _packageSearcher = new PackageSearcher();
    private final Joiner _joiner;

    public Reconciliation(AnnotationAccessor annotationAccessor)
    {
        _joiner = new Joiner(annotationAccessor);
    }

    public ReconciliationReport reconcile(List<String> protonCFunctions, String packageRootName)
    {
        Set<Method> javaMethods = _packageSearcher.findMethods(packageRootName);
        ReconciliationReport report = _joiner.join(protonCFunctions, javaMethods);
        return report;
    }

}
