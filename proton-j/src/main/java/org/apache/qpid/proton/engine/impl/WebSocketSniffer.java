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

package org.apache.qpid.proton.engine.impl;

public class WebSocketSniffer extends HandshakeSniffingTransportWrapper<TransportWrapper, TransportWrapper>
{
    public WebSocketSniffer(TransportWrapper webSocket, TransportWrapper other) {
        super(webSocket, other);
    }

    @Override
    protected int bufferSize()
    {
        return WebSocketHeader.HEADER_SIZE;
    }

    @Override
    protected void makeDetermination(byte[] bytes)
    {
        if (bytes.length < bufferSize()) {
            throw new IllegalArgumentException("insufficient bytes");
        }

        if (bytes[0] != WebSocketHeader.FINALBINARYHEADER[0])
        {
            _selectedTransportWrapper = _wrapper2;
            return;
        }

        _selectedTransportWrapper = _wrapper1;
    }
}
