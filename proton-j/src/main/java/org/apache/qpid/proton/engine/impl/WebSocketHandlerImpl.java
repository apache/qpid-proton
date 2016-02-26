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

package org.apache.qpid.proton.engine.impl;

import org.apache.qpid.proton.engine.WebSocketHandler;

import java.io.*;
import java.net.URI;
import java.nio.ByteBuffer;
import java.security.SecureRandom;
import java.util.Map;
import java.util.Random;

public class WebSocketHandlerImpl implements WebSocketHandler
{
    @Override
    public String createUpgradeRequest(
            String hostName,
            String webSocketPath,
            int webSocketPort,
            String webSocketProtocol,
            Map<String, String> additionalHeaders)

    {
        WebSocketUpgradeRequest webSocketUpgradeRequest = new WebSocketUpgradeRequest(hostName, webSocketPath, webSocketPort, webSocketProtocol, additionalHeaders);
        return webSocketUpgradeRequest.createUpgradeRequest();
    }

    @Override
    public Boolean validateUpgradeReply(ByteBuffer buffer)
    {
        return false;
    }

    @Override
    public void wrapBuffer(ByteBuffer srcBuffer, ByteBuffer dstBuffer)
    {
    }

    @Override
    public void unwrapBuffer(ByteBuffer buffer)
    {
    }

    @Override
    public void createPong(ByteBuffer srcBuffer, ByteBuffer dstBuffer)
    {
    }
}
