package org.apache.qpid.proton.engine;
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


public interface SaslClient extends Sasl
{
    /** Configure the SASL layer to use the "PLAIN" mechanism.
     *
     * A utility function to configure a simple client SASL layer using
     * PLAIN authentication.
     *
     * @param username credential for the PLAIN authentication
     *                     mechanism
     * @param password credential for the PLAIN authentication
     *                     mechanism
     */
    void plain(String username, String password);

    /** Retrieve the outcome of SASL negotiation.
     *
     */
    SaslOutcome getOutcome();


}
