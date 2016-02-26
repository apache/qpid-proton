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

import org.apache.qpid.proton.engine.impl.TransportInput;
import org.apache.qpid.proton.engine.impl.TransportOutput;
import org.apache.qpid.proton.engine.impl.TransportWrapper;

import java.nio.ByteBuffer;

/*
 * Provides interface for WebSocket
 */
public interface WebSocket
{
    public enum WebSocketState
    {
        /** WebSocket */
        PN_WS_NOT_STARTED,
        /** Pending connection */
        PN_WS_CONNECTING,
        /** Connected */
        PN_WS_CONNECTED,
        /** Connection closed */
        PN_WS_CLOSED,
        /** Connection failed */
        PN_WS_FAILED
    }


    /**
     * Add WebSocket frame to send the given buffer
     *
     * @return The WebSocket frame to send.
     */
    void wrapBuffer(ByteBuffer srcBuffer, ByteBuffer dstBuffer);

    /**
     * Remove WebSocket frame from the given buffer
     *
     * @return The payload of the given WebSocket frame.
     */
    void unwrapBuffer(ByteBuffer buffer);

    /**
     * Access the current state of the layer.
     *
     * @return The state of the WebSocket layer.
     */
    WebSocketState getState();

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
     * @return The number of bytes written to bytes, or an error code if {@literal < 0}.
     */
    int recv(byte[] bytes, int offset, int size);

    /**
     * Send challenge or response data to the peer.
     *
     * @param bytes The challenge/response data.
     * @param offset the point within the array at which the data starts at
     * @param size The number of data octets in bytes.
     * @return The number of octets read from bytes, or an error code if {@literal < 0}
     */
    int send(byte[] bytes, int offset, int size);

}
