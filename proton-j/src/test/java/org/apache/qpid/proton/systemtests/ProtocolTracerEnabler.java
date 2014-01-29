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

import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.util.logging.Logger;

import org.apache.qpid.proton.engine.Transport;

public class ProtocolTracerEnabler
{
    private static final Logger LOGGER = Logger.getLogger(ProtocolTracerEnabler.class.getName());

    private static final String LOGGING_PROTOCOL_TRACER_CLASS_NAME = "org.apache.qpid.proton.logging.LoggingProtocolTracer";

    /**
     * Attempts to set up a {@value #LOGGING_PROTOCOL_TRACER_CLASS_NAME} on the supplied transport.
     * Uses reflection so this code can be run without a compile-time dependency on proton-j-impl.
     */
    public static void setProtocolTracer(Transport transport, String prefix)
    {
        try
        {
            Class<?> loggingProtocolTracerClass = Class.forName(LOGGING_PROTOCOL_TRACER_CLASS_NAME);

            Constructor<?> loggingProtocolTracerConstructor = loggingProtocolTracerClass.getConstructor(String.class);
            Object newLoggingProtocolTracer = loggingProtocolTracerConstructor.newInstance(prefix);

            Class<?> protocolTracerClass = Class.forName("org.apache.qpid.proton.engine.impl.ProtocolTracer");
            Method setPrococolTracerMethod = transport.getClass().getMethod("setProtocolTracer", protocolTracerClass);

            setPrococolTracerMethod.invoke(transport, newLoggingProtocolTracer);
        }
        catch(Exception e)
        {
            if(e instanceof ClassNotFoundException || e instanceof NoSuchMethodException)
            {
                LOGGER.fine("Protocol tracing disabled because unable to reflectively set a "
                        + LOGGING_PROTOCOL_TRACER_CLASS_NAME + " instance on the supplied transport which is a: "
                        + transport.getClass().getName());
            }
            else
            {
                throw new RuntimeException("Unable to set up protocol tracing", e);
            }
        }
    }
}
