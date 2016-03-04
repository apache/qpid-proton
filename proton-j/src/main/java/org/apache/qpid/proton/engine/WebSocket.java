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
        /** Connected and messages flow */
        PN_WS_CONNECTED_FLOW,
        /** Connected and ping-pong */
        PN_WS_CONNECTED_PONG,
        /** Connection closed */
        PN_WS_CLOSED,
        /** Connection failed */
        PN_WS_FAILED
    }


    /**
     * Add WebSocket frame to send the given buffer
     */
    void wrapBuffer(ByteBuffer srcBuffer, ByteBuffer dstBuffer);

    /**
     * Remove WebSocket frame from the given buffer
     *
     * @return The payload of the given WebSocket frame.
     */
    WebSocketHandler.WebSocketMessageType unwrapBuffer(ByteBuffer buffer);

    /**
     * Access the current state of the layer.
     *
     * @return The state of the WebSocket layer.
     */
    WebSocketState getState();
}
