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


public interface Sasl
{
    public enum SaslState
    {
        /** Pending configuration by application */
        PN_SASL_CONF,
        /** Pending SASL Init */
        PN_SASL_IDLE,
        /** negotiation in progress */
        PN_SASL_STEP,
        /** negotiation completed successfully */
        PN_SASL_PASS,
        /** negotiation failed */
        PN_SASL_FAIL
    }

    public enum SaslOutcome
    {
        /** negotiation not completed */
        PN_SASL_NONE((byte)-1),
        /** authentication succeeded */
        PN_SASL_OK((byte)0),
        /** failed due to bad credentials */
        PN_SASL_AUTH((byte)1),
        /** failed due to a system error */
        PN_SASL_SYS((byte)2),
        /** failed due to unrecoverable error */
        PN_SASL_PERM((byte)3),
        PN_SASL_TEMP((byte)4),
        PN_SASL_SKIPPED((byte)5);

        private final byte _code;

        /** failed due to transient error */

        SaslOutcome(byte code)
        {
            _code = code;
        }

        public byte getCode()
        {
            return _code;
        }
    }

    public static SaslOutcome PN_SASL_NONE = SaslOutcome.PN_SASL_NONE;
    public static SaslOutcome PN_SASL_OK = SaslOutcome.PN_SASL_OK;
    public static SaslOutcome PN_SASL_AUTH = SaslOutcome.PN_SASL_AUTH;
    public static SaslOutcome PN_SASL_SYS = SaslOutcome.PN_SASL_SYS;
    public static SaslOutcome PN_SASL_PERM = SaslOutcome.PN_SASL_PERM;
    public static SaslOutcome PN_SASL_TEMP = SaslOutcome.PN_SASL_TEMP;
    public static SaslOutcome PN_SASL_SKIPPED = SaslOutcome.PN_SASL_SKIPPED;

    /**
     * Access the current state of the layer.
     *
     * @return The state of the sasl layer.
     */
    SaslState getState();

    /**
     * Set the acceptable SASL mechanisms for the layer.
     *
     * @param mechanisms a list of acceptable SASL mechanisms
     */
    void setMechanisms(String... mechanisms);

    /**
     * Retrieve the list of SASL mechanisms provided by the remote.
     *
     * @return the SASL mechanisms advertised by the remote
     */
    String[] getRemoteMechanisms();

    /**
     * Determine the size of the bytes available via recv().
     *
     * Returns the size in bytes available via recv().
     *
     * @return The number of bytes available, zero if no available data.
     */
    int pending();

    /**
     * Read challenge/response data sent from the peer.
     *
     * Use pending to determine the size of the data.
     *
     * @param bytes written with up to size bytes of inbound data.
     * @param offset the offset in the array to begin writing at
     * @param size maximum number of bytes that bytes can accept.
     * @return The number of bytes written to bytes, or an error code if < 0.
     */
    int recv(byte[] bytes, int offset, int size);

    /**
     * Send challenge or response data to the peer.
     *
     * @param bytes The challenge/response data.
     * @param offset the point within the array at which the data starts at
     * @param size The number of data octets in bytes.
     * @return The number of octets read from bytes, or an error code if < 0
     */
    int send(byte[] bytes, int offset, int size);


    /**
     * Set the outcome of SASL negotiation
     *
     * Used by the server to set the result of the negotiation process.
     *
     * @todo
     */
    void done(SaslOutcome outcome);


    /**
     * Configure the SASL layer to use the "PLAIN" mechanism.
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

    /**
     * Retrieve the outcome of SASL negotiation.
     */
    SaslOutcome getOutcome();

    void client();
    void server();

    /**
     * Set whether servers may accept incoming connections
     * that skip the SASL layer negotiation.
     */
    void allowSkip(boolean allowSkip);
}
