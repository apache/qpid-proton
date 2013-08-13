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

package org.apache.qpid.proton.engine;

import java.util.IllegalFormatException;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.apache.qpid.proton.ProtonException;

public class TransportException extends ProtonException
{

    private static final Logger LOGGER = Logger.getLogger(TransportException.class.getName());

    public TransportException()
    {
    }

    public TransportException(String message)
    {
        super(message);
    }

    public TransportException(String message, Throwable cause)
    {
        super(message, cause);
    }

    public TransportException(Throwable cause)
    {
        super(cause);
    }

    private static String format(String format, Object ... args)
    {
        try
        {
            return String.format(format, args);
        }
        catch(IllegalFormatException e)
        {
            LOGGER.log(Level.SEVERE, "Formating error in string " + format, e);
            return format;
        }
    }

    public TransportException(String format, Object ... args)
    {
        this(format(format, args));
    }

}
