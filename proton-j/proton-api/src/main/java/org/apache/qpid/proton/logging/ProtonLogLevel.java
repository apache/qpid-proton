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

/**
 * The level of a log message
 */
public enum ProtonLogLevel
{
    /** potentially byte-by-byte output */
    TRACE,

    DEBUG,

    /** Typical example: a normal lifecycle event has occurred on an endpoint */
    INFO,

    /** May indicate a problem */
    WARN,

    /**
     * Indicates a problem which is either (1) invalid input or (2) a Proton internal error.
     */
    ERROR,

    ;
}
