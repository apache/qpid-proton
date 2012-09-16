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
package org.apache.qpid.proton.test;

import org.junit.Test;
import static org.junit.Assert.*;
import org.python.util.PythonInterpreter;
import java.io.File;

/**
 * Runs all the python tests.
 */
public class JythonTest
{

    static final private String PROTON_TESTS = "PROTON_TESTS";

    @Test
    public void test() throws Exception
    {

        File basedir = new File(getClass().getProtectionDomain().getCodeSource().getLocation().getFile(), "../..").getCanonicalFile();
        File testDir;
        String protonTestsVar = System.getenv(PROTON_TESTS);
        if( protonTestsVar != null && protonTestsVar.trim().length()>0 )
        {
            testDir = new File(protonTestsVar).getCanonicalFile();
            assertTrue(PROTON_TESTS + " env variable set incorrectly: " + protonTestsVar, testDir.isDirectory());
        }
        else
        {
            testDir = new File(basedir, "../tests");
            if( !testDir.isDirectory() )
            {
                // The tests might not be there if the proton-j module is released independently
                // from the main proton project.
                return;
            }
        }

        File classesDir = new File(basedir, "target/classes");
        PythonInterpreter interp = new PythonInterpreter();

        interp.exec(
        "import sys\n"+
        "sys.path.insert(0,\""+classesDir.getCanonicalPath()+"\")\n"+
        "sys.path.insert(0,\""+testDir.getCanonicalPath()+"\")\n"
        );
        interp.execfile(new File(testDir, "proton-test").getCanonicalPath());

    }

}
