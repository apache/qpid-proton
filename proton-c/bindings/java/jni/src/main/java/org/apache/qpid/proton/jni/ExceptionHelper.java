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
package org.apache.qpid.proton.jni;

import java.util.logging.Level;
import java.util.logging.Logger;

public class ExceptionHelper
{
    private static final Logger LOGGER = Logger.getLogger(ExceptionHelper.class.getName());

    /**
     * Check the return value from a Proton function call and throw
     * an exception if it's non-zero.
     * @throws JNIException
     */
    public static void checkProtonCReturnValue(int retVal)
    {
        if(retVal != 0)
        {
            if(LOGGER.isLoggable(Level.FINE))
            {
                LOGGER.log(Level.FINE, "Non-zero return value: " + retVal, new Exception("<dummy exception to generate stack trace>"));
            }
            throw new JNIException(retVal);
        }
    }
}
