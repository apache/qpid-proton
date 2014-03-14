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
package org.apache.qpid.proton;

/**
 * Use to indicate that a feature of the Proton API is not supported by a particular implementation
 * (e.g. proton-j or proton-c-via-JNI).
 */
public class ProtonUnsupportedOperationException extends UnsupportedOperationException
{
    /** Used by the Python test layer to detect an unsupported operation */
    public static final boolean skipped = true;

    public ProtonUnsupportedOperationException()
    {
    }

    public ProtonUnsupportedOperationException(String message)
    {
        super(message);
    }

    public ProtonUnsupportedOperationException(String message, Throwable cause)
    {
        super(message, cause);
    }

    public ProtonUnsupportedOperationException(Throwable cause)
    {
        super(cause);
    }
}
