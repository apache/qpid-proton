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
package org.apache.qpid.proton.engine;

import static org.apache.qpid.proton.engine.TransportResult.Status.ERROR;
import static org.apache.qpid.proton.engine.TransportResult.Status.OK;

import java.util.IllegalFormatException;
import java.util.logging.Level;
import java.util.logging.Logger;


/**
 * Creates TransportResults.
 * Only intended for use by internal Proton classes.
 * This class resides in the api module so it can be used by both proton-j-impl and proton-jni.
 */
public class TransportResultFactory
{
    private static final Logger LOGGER = Logger.getLogger(TransportResultFactory.class.getName());

    private static final TransportResult _okResult = new TransportResultImpl(OK, null, null);

    public static TransportResult ok()
    {
        return _okResult;
    }

    public static TransportResult error(String format, Object... args)
    {
        String errorDescription;
        try
        {
            errorDescription = String.format(format, args);
        }
        catch(IllegalFormatException e)
        {
            LOGGER.log(Level.SEVERE, "Formating error in string " + format, e);
            errorDescription = format;
        }
        return new TransportResultImpl(ERROR, errorDescription, null);
    }

    public static TransportResult error(final String errorDescription)
    {
        return new TransportResultImpl(ERROR, errorDescription, null);
    }

    public static TransportResult error(final Exception e)
    {
        return new TransportResultImpl(ERROR, e == null ? null : e.toString(), e);
    }

    private static final class TransportResultImpl implements TransportResult
    {
        private final String _errorDescription;
        private final Status _status;
        private final Exception _exception;

        private TransportResultImpl(Status status, String errorDescription, Exception exception)
        {
            _status = status;
            _errorDescription = errorDescription;
            _exception = exception;
        }

        @Override
        public boolean isOk()
        {
            return _status == OK;
        }

        @Override
        public Status getStatus()
        {
            return _status;
        }

        @Override
        public String getErrorDescription()
        {
            return _errorDescription;
        }

        @Override
        public Exception getException()
        {
            return _exception;
        }

        @Override
        public void checkIsOk()
        {
            if (!isOk())
            {
                Exception e = getException();
                if (e != null)
                {
                    throw new TransportException(e);
                }
                else
                {
                    throw new TransportException(getErrorDescription());
                }
            }
        }
    }
}
