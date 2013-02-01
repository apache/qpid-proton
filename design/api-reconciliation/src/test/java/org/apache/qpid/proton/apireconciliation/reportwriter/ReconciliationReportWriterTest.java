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

import java.io.File;
import java.io.IOException;
import java.net.URISyntaxException;
import java.net.URL;

import org.apache.commons.io.FileUtils;
import org.apache.qpid.proton.apireconciliation.ReconciliationReport;
import org.apache.qpid.proton.apireconciliation.TestAnnotation;
import org.junit.Before;
import org.junit.Test;


public class ReconciliationReportWriterTest
{
    private ReconciliationReportWriter _writer;
    private ReconciliationReport _report = new ReconciliationReport();

    @Before
    public void setUp()
    {
        _writer = new ReconciliationReportWriter(new AnnotationAccessor(TestAnnotation.class.getName()));
    }

    @Test
    public void testReportWithSingleFullyMappedRow() throws Exception
    {
        File expectedReport = getClasspathResource("expectedsingle.csv");
        File outputFile = createTemporaryFile();

        _report.addRow("function1", getClass().getMethod("methodWithMapping"));
        _writer.write(outputFile.getAbsolutePath(), _report);

        assertFilesSame(expectedReport, outputFile);
    }

    @Test
    public void testReportWithManyRowsSomeUnmapped() throws Exception
    {
        File expectedReport = getClasspathResource("expectedmany.csv");
        File outputFile = createTemporaryFile();

        _report.addRow("function1", getClass().getMethod("methodWithMapping"));
        _report.addRow("function2", getClass().getMethod("anotherMethodWithMapping"));
        _report.addRow(null, getClass().getMethod("methodWithoutMapping"));
        _report.addRow("function4", null);
        _writer.write(outputFile.getAbsolutePath(), _report);

        assertFilesSame(expectedReport, outputFile);
    }

    private File getClasspathResource(String filename) throws URISyntaxException
    {
        URL resource = getClass().getResource(filename);
        assertNotNull("Resource " + filename + " could not be found",resource);
        return new File(resource.toURI());
    }

    private File createTemporaryFile() throws Exception
    {
        File tmpFile = File.createTempFile(getClass().getSimpleName(), "csv");
        tmpFile.deleteOnExit();
        return tmpFile;
    }

    private void assertFilesSame(File expectedReport, File actualReport) throws IOException
    {
        assertTrue(expectedReport.canRead());
        assertTrue(actualReport.canRead());
        assertEquals("Report contents unexpected",
                FileUtils.readFileToString(expectedReport),
                FileUtils.readFileToString(actualReport));
    }

    @TestAnnotation("function1")
    public void methodWithMapping()
    {
    }

    @TestAnnotation("function2")
    public void anotherMethodWithMapping()
    {
    }

    public void methodWithoutMapping()
    {
    }
}
