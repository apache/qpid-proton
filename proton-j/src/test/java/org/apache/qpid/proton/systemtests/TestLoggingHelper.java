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
package org.apache.qpid.proton.systemtests;

import java.nio.ByteBuffer;
import java.util.logging.Logger;

/**
 * Provides functions for Proton tests to produce readable logging.
 * Uses terminal colours if system property {@value #PROTON_TEST_TERMINAL_COLOURS_PROPERTY}
 * is true
 */
public class TestLoggingHelper
{
    public static final String PROTON_TEST_TERMINAL_COLOURS_PROPERTY = "proton.test.terminal.colours";

    private static final String COLOUR_RESET;
    private static final String SERVER_COLOUR;
    private static final String CLIENT_COLOUR;
    private static final String BOLD;

    static
    {
        if(Boolean.getBoolean(PROTON_TEST_TERMINAL_COLOURS_PROPERTY))
        {
            BOLD = "\033[1m";
            SERVER_COLOUR = "\033[34m"; // blue
            CLIENT_COLOUR = "\033[32m"; // green
            COLOUR_RESET = "\033[0m";
        }
        else
        {
            BOLD = SERVER_COLOUR = CLIENT_COLOUR = COLOUR_RESET = "";
        }
    }

    public static final String CLIENT_PREFIX = CLIENT_COLOUR + "CLIENT" + COLOUR_RESET;
    public static final String SERVER_PREFIX = SERVER_COLOUR + "SERVER" + COLOUR_RESET;
    public static final String MESSAGE_PREFIX = "MESSAGE";


    private final BinaryFormatter _binaryFormatter = new BinaryFormatter();
    private Logger _logger;

    public TestLoggingHelper(Logger logger)
    {
        _logger = logger;
    }

    public void prettyPrint(String prefix, byte[] bytes)
    {
        _logger.fine(prefix + " " + bytes.length + " byte(s) " + _binaryFormatter.format(bytes));
    }

    /**
     * Note that ByteBuffer is assumed to be readable. Its state is unchanged by this operation.
     */
    public void prettyPrint(String prefix, ByteBuffer buf)
    {
        byte[] bytes = new byte[buf.remaining()];
        buf.duplicate().get(bytes);
        prettyPrint(prefix, bytes);
    }

    public static String bold(String string)
    {
        return BOLD + string + TestLoggingHelper.COLOUR_RESET;
    }


}
