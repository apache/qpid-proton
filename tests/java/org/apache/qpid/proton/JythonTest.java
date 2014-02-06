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

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.junit.Test;
import org.python.core.PyException;
import org.python.core.PyString;
import org.python.core.PySystemState;
import org.python.util.PythonInterpreter;

/**
 * Runs all the python tests, or just those that match the system property {@value #TEST_PATTERN_SYSTEM_PROPERTY}
 * if it exists.
 * Use {@value #TEST_INVOCATIONS_SYSTEM_PROPERTY} to specify the number of repetitions, or use 0
 * for unlimited repetitions.
 */
public class JythonTest
{
    private static final Logger LOGGER = Logger.getLogger(JythonTest.class.getName());

    /* System properties expected to be defined in test/pom.xml */
    private static final String PROTON_JYTHON_BINDING = "protonJythonBinding";
    private static final String PROTON_JYTHON_TEST_ROOT = "protonJythonTestRoot";
    private static final String PROTON_JYTHON_TEST_SCRIPT = "protonJythonTestScript";
    private static final String PROTON_JYTHON_TESTS_XML_OUTPUT_DIRECTORY = "protonJythonTestXmlOutputDirectory";
    private static final String PROTON_JYTHON_IGNORE_FILE = "protonJythonIgnoreFile";

    /** Name of the junit style xml report to be written by the python test script */
    private static final String XML_REPORT_NAME = "TEST-jython-test-results.xml";

    public static final String TEST_PATTERN_SYSTEM_PROPERTY = "proton.pythontest.pattern";
    public static final String IGNORE_FILE_SYSTEM_PROPERTY = "proton.pythontest.ignore_file";

    /** The number of times to run the test, or forever if zero */
    public static final String TEST_INVOCATIONS_SYSTEM_PROPERTY = "proton.pythontest.invocations";

    public static final String ALWAYS_COLORIZE_SYSTEM_PROPERTY = "proton.pythontest.always_colorize";

    @Test
    public void test() throws Exception
    {
        String testScript = getJythonTestScript();
        String binding = getJythonBinding();
        String testRoot = getJythonTestRoot();
        String xmlReportFile = getOptionalXmlReportFilename();
        String ignoreFile = getOptionalIgnoreFile();

        PythonInterpreter interp = createInterpreterWithArgs(xmlReportFile, ignoreFile);
        interp.getSystemState().path.insert(0, new PyString(binding));
        interp.getSystemState().path.insert(0, new PyString(testRoot));

        LOGGER.info("About to call Jython test script: '" + testScript
                + "' with '" + testRoot + "' added to Jython path");

        int maxInvocations = Integer.getInteger(TEST_INVOCATIONS_SYSTEM_PROPERTY, 1);
        assertTrue("Number of invocations should be non-negative", maxInvocations >= 0);
        boolean loopForever = maxInvocations == 0;
        if(maxInvocations > 1)
        {
            LOGGER.info("Will invoke Python test " + maxInvocations + " times");
        }
        if(loopForever)
        {
            LOGGER.info("Will repeatedly invoke Python test forever");
        }
        int invocations = 1;
        while(loopForever || invocations++ <= maxInvocations)
        {
            runTestOnce(testScript, interp, invocations);
        }
    }

    private void runTestOnce(String testScript, PythonInterpreter interp, int invocationsSoFar)
    {
        try
        {
            interp.execfile(testScript);
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
                fail("Caught PyException on invocation number " + invocationsSoFar + ": " + e.toString() + " with message: " + e.getMessage());
            }
        }
    }

    private PythonInterpreter createInterpreterWithArgs(String xmlReportFile, String ignoreFile)
    {
        PySystemState systemState = new PySystemState();

        if (xmlReportFile != null)
        {
            systemState.argv.append(new PyString("--xml"));
            systemState.argv.append(new PyString(xmlReportFile));
        }

        if(ignoreFile == null)
        {
            ignoreFile = System.getProperty(IGNORE_FILE_SYSTEM_PROPERTY);
        }

        if(ignoreFile != null)
        {
            systemState.argv.append(new PyString("-I"));
            systemState.argv.append(new PyString(ignoreFile));
        }

        String testPattern = System.getProperty(TEST_PATTERN_SYSTEM_PROPERTY);
        if(testPattern != null)
        {
            systemState.argv.append(new PyString(testPattern));
        }

        if(Boolean.getBoolean(ALWAYS_COLORIZE_SYSTEM_PROPERTY))
        {
            systemState.argv.append(new PyString("--always-colorize"));
        }

        PythonInterpreter interp = new PythonInterpreter(null, systemState);
        return interp;
    }

    private String getJythonTestScript() throws FileNotFoundException
    {
        String testScriptString = getNonNullSystemProperty(PROTON_JYTHON_TEST_SCRIPT, "System property '%s' must provide the location of the python test script");
        File testScript = new File(testScriptString);
        if (!testScript.canRead())
        {
            throw new FileNotFoundException("Can't read python test script " + testScript);
        }
        return testScript.getAbsolutePath();
    }

    private String getJythonBinding() throws FileNotFoundException
    {
        String str = getNonNullSystemProperty(PROTON_JYTHON_BINDING, "System property '%s' must provide the location of the python test root");
        File file = new File(str);
        if (!file.isDirectory())
        {
            throw new FileNotFoundException("Binding location '" + file + "' should be a directory.");
        }
        return file.getAbsolutePath();
    }


    private String getJythonTestRoot() throws FileNotFoundException
    {
        String testRootString = getNonNullSystemProperty(PROTON_JYTHON_TEST_ROOT, "System property '%s' must provide the location of the python test root");
        File testRoot = new File(testRootString);
        if (!testRoot.isDirectory())
        {
            throw new FileNotFoundException("Test root '" + testRoot + "' should be a directory.");
        }
        return testRoot.getAbsolutePath();
    }

    private String getOptionalIgnoreFile()
    {
        String ignoreFile = System.getProperty(PROTON_JYTHON_IGNORE_FILE);

        if(ignoreFile != null)
        {
            File f = new File(ignoreFile);
            if(f.exists() && f.canRead())
            {
                return ignoreFile;
            }
            else
            {
                LOGGER.info(PROTON_JYTHON_IGNORE_FILE + " system property set to " + ignoreFile + " but this cannot be read.");
            }
        }
        return null;
        
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

    private String getNonNullSystemProperty(String systemProperty, String messageWithStringFormatToken)
    {
        String testScriptString = System.getProperty(systemProperty);
        if (testScriptString == null)
        {
            String message = messageWithStringFormatToken;
            throw new IllegalStateException(String.format(message, systemProperty));
        }
        return testScriptString;
    }
}
