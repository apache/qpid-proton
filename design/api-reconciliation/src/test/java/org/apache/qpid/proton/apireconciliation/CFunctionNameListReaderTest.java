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

import static org.junit.Assert.*;

import java.io.File;
import java.util.Arrays;
import java.util.List;

import org.apache.commons.io.FileUtils;
import org.junit.Test;

public class CFunctionNameListReaderTest
{

    private CFunctionNameListReader _cFunctionDeclarationReader = new CFunctionNameListReader();

    @Test
    public void testReadFileContainingSingleCFunction() throws Exception
    {
        String declarationFile = createTestFileContaining("function1", "function2", "function3");

        List<String> functions = _cFunctionDeclarationReader.readCFunctionNames(declarationFile);
        assertEquals(3, functions.size());
        assertEquals("function1", functions.get(0));
        assertEquals("function3", functions.get(2));
    }

    private String createTestFileContaining(String... functionNames) throws Exception
    {
        File file = File.createTempFile(CFunctionNameListReader.class.getSimpleName(), "txt");
        file.deleteOnExit();
        FileUtils.writeLines(file, Arrays.asList(functionNames));
        return file.getAbsolutePath();
    }
}
