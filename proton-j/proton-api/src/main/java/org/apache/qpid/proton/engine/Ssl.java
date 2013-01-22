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

/**
 * I represent the details of a particular SSL session.
 */
public interface Ssl
{
    /**
     * Get the name of the Cipher that is currently in use.
     *
     * Gets a text description of the cipher that is currently active, or returns null if SSL
     * is not active (no cipher). Note that the cipher in use may change over time due to
     * renegotiation or other changes to the SSL state.
     *
     * @return the name of the cipher in use, or null if none
     */
    String getCipherName();

    /**
     * Get the name of the SSL protocol that is currently in use.
     *
     * Gets a text description of the SSL protocol that is currently active, or null if SSL
     * is not active. Note that the protocol may change over time due to renegotiation.
     *
     * @return the name of the protocol in use, or null if none
     */
    String getProtocolName();

    void setPeerHostname(String hostname);

    String getPeerHostname();
}
