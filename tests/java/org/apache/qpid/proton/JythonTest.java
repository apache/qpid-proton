/*
 *
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
package org.apache.qpid.proton;

import static org.junit.Assert.fail;

import java.io.File;
import java.net.URISyntaxException;
import java.net.URL;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.junit.Test;
import org.python.core.PyException;
import org.python.core.PyString;
import org.python.core.PySystemState;
import org.python.util.PythonInterpreter;

/**
 * Runs all the python tests, or just those that match the system property {@value #TEST_PATTERN_SYSTEM_PROPERTY}
 * if it exists
 */
public class JythonTest
{

    private static final Logger LOGGER = Logger.getLogger(JythonTest.class.getName());

    /** System property is defined in test/pom.xml */
    private static final String PROTON_JYTHON_TESTS_XML_OUTPUT_DIRECTORY = "protonJythonTestsXmlOutputDirectory";
    /** Name of the junit style xml report to be written by the python test script */
    private static final String XML_REPORT_NAME = "TEST-jython-test-results.xml";

    private static final String TEST_PATTERN_SYSTEM_PROPERTY = "proton.pythontest.pattern";
    private static final String PROTON_TEST_SCRIPT_CLASSPATH_LOCATION = "/proton-test";

    @Test
    public void test() throws Exception
    {
        File protonScriptFile = getPythonTestScript();
        String parentDirectory = protonScriptFile.getParent();
        String xmlReportFile = getOptionalXmlReportFilename();

        PythonInterpreter interp = createInterpreterWithArgs(xmlReportFile);

        LOGGER.info("About to call Jython test script: " + protonScriptFile + " with parent directory added to Jython path");

        interp.exec(
        "import sys\n"+
        "sys.path.insert(0,\""+parentDirectory+"\")\n"
        );

        try
        {
            String protonTestPyPath = protonScriptFile.getAbsolutePath();
            interp.execfile(protonTestPyPath);
        }
        catch (PyException e)
        {
            if( e.type.toString().equals("<type 'exceptions.SystemExit'>") && e.value.toString().equals("0") )
            {
                // Build succeeded.
            }
            else
            {
                if (LOGGER.isLoggable(Level.FINE))
                {
                    LOGGER.log(Level.FINE, "Jython interpreter failed. Test failures?", e);
                }

                // This unusual code is necessary because PyException toString() contains the useful Python traceback
                // and getMessage() is usually null
                fail("Caught PyException: " + e.toString() + " with message: " + e.getMessage());
            }
        }
    }

    private PythonInterpreter createInterpreterWithArgs(String xmlReportFile)
    {
        PySystemState systemState = new PySystemState();
        String testPattern = System.getProperty(TEST_PATTERN_SYSTEM_PROPERTY);

        if (xmlReportFile != null)
        {
            systemState.argv.append(new PyString("--xml"));
            systemState.argv.append(new PyString(xmlReportFile));
        }

        if(testPattern != null)
        {
            systemState.argv.append(new PyString(testPattern));
        }

        PythonInterpreter interp = new PythonInterpreter(null, systemState);
        return interp;
    }

    private File getPythonTestScript() throws URISyntaxException
    {
        URL protonScriptUrl = getClass().getResource(PROTON_TEST_SCRIPT_CLASSPATH_LOCATION);
        File protonScriptFile = new File(protonScriptUrl.toURI());
        return protonScriptFile;
    }

    private String getOptionalXmlReportFilename()
    {
        String xmlOutputDirString = System.getProperty(PROTON_JYTHON_TESTS_XML_OUTPUT_DIRECTORY);
        if (xmlOutputDirString == null)
        {
            LOGGER.info(PROTON_JYTHON_TESTS_XML_OUTPUT_DIRECTORY + " system property not set; xml output will not be written");
        }

        File xmlOutputDir = new File(xmlOutputDirString);
        createXmlOutputDirectoryIfNecessary(xmlOutputDirString, xmlOutputDir);
        return new File(xmlOutputDir, XML_REPORT_NAME).getAbsolutePath();
    }

    private void createXmlOutputDirectoryIfNecessary(String xmlOutputDirString, File xmlOutputDir)
    {
        if (!xmlOutputDir.isDirectory())
        {
            boolean success = xmlOutputDir.mkdirs();
            if (!success)
            {
                LOGGER.warning("Failed to create directory " + xmlOutputDir + " Thread name :" + Thread.currentThread().getName());
            }

            if (!xmlOutputDir.isDirectory())
            {
                throw new RuntimeException("Failed to create one or more directories with path " + xmlOutputDirString);
            }
        }
    }
}
