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
package org.apache.qpid.proton.logging;

import java.util.logging.Logger;

import org.apache.qpid.proton.engine.impl.ProtocolTracer;
import org.apache.qpid.proton.framing.TransportFrame;

public class LoggingProtocolTracer implements ProtocolTracer
{
    private static final String LOGGER_NAME_STEM = LoggingProtocolTracer.class.getName();

    private static final Logger RECEIVED_LOGGER = Logger.getLogger(LOGGER_NAME_STEM + ".received");
    private static final Logger SENT_LOGGER = Logger.getLogger(LOGGER_NAME_STEM + ".sent");

    private String _logMessagePrefix;

    public LoggingProtocolTracer()
    {
        this("Transport");
    }

    public LoggingProtocolTracer(String logMessagePrefix)
    {
        _logMessagePrefix = logMessagePrefix;
    }

    @Override
    public void receivedFrame(TransportFrame transportFrame)
    {
        RECEIVED_LOGGER.finer(_logMessagePrefix + " received frame: " + transportFrame);
    }

    @Override
    public void sentFrame(TransportFrame transportFrame)
    {
        SENT_LOGGER.finer(_logMessagePrefix + " writing frame: " + transportFrame);
    }

    public void setLogMessagePrefix(String logMessagePrefix)
    {
        _logMessagePrefix = logMessagePrefix;
    }
}
