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


/**
 * Transport
 *
 */

public interface Transport extends Endpoint
{

    public int END_OF_STREAM = -1;

    public void bind(Connection connection);

    /**
     * @param bytes input bytes for consumption
     * @param offset the offset within bytes where input begins
     * @param size the number of bytes available for input
     *
     * @return the number of bytes consumed
     */
    public int input(byte[] bytes, int offset, int size);

    /**
     * Has the transport produce up to size bytes placing the result
     * into dest beginning at position offset.
     *
     *
     *
     * @param dest array for output bytes
     * @param offset the offset within bytes where output begins
     * @param size the maximum number of bytes to be output
     *
     * @return the number of bytes written
     */
    public int output(byte[] dest, int offset, int size);


    Sasl sasl();

    /**
     * Wrap this transport's output and input to apply SSL encryption and decryption respectively.
     *
     * This method is expected to be called at most once. A subsequent invocation will return the same
     * {@link Ssl} object, regardless of the parameters supplied.
     *
     * @param sslDomain the SSL settings to use
     * @param sslPeerDetails may be null, in which case SSL session resume will not be attempted
     * @return an {@link Ssl} object representing the SSL session.
     */
    Ssl ssl(SslDomain sslDomain, SslPeerDetails sslPeerDetails);

    /**
     * As per {@link #ssl(SslDomain, SslPeerDetails)} but no attempt is made to resume a previous SSL session.
     */
    Ssl ssl(SslDomain sslDomain);
}
