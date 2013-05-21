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
package org.apache.qpid.proton.engine.impl.ssl;

import java.nio.ByteBuffer;

import javax.net.ssl.SSLEngine;
import javax.net.ssl.SSLEngineResult;
import javax.net.ssl.SSLEngineResult.HandshakeStatus;
import javax.net.ssl.SSLEngineResult.Status;
import javax.net.ssl.SSLException;

/**
 * Thin wrapper around an {@link SSLEngine}.
 */
public interface ProtonSslEngine
{
    /**
     * @see SSLEngine#wrap(ByteBuffer, ByteBuffer)
     *
     * Note that wrap really does write <em>one</em> packet worth of data to the
     * dst byte buffer.  If dst byte buffer is insufficiently large the
     * pointers within both src and dst are unchanged and the bytesConsumed and
     * bytesProduced on the returned result are zero.
     */
    SSLEngineResult wrap(ByteBuffer src, ByteBuffer dst) throws SSLException;

    /**
     * @see SSLEngine#unwrap(ByteBuffer, ByteBuffer)
     *
     * Note that unwrap does read exactly one packet of encoded data from src
     * and write to dst.  If src contains insufficient bytes to read a complete
     * packet {@link Status#BUFFER_UNDERFLOW} occurs.  If underflow occurs the
     * pointers within both src and dst are unchanged and the bytesConsumed and
     * bytesProduced on the returned result are zero.
    */
    SSLEngineResult unwrap(ByteBuffer src, ByteBuffer dst) throws SSLException;

    Runnable getDelegatedTask();
    HandshakeStatus getHandshakeStatus();

    /**
     * Gets the application buffer size.
     */
    int getEffectiveApplicationBufferSize();

    int getPacketBufferSize();
    String getCipherSuite();
    String getProtocol();
    boolean getUseClientMode();

}
