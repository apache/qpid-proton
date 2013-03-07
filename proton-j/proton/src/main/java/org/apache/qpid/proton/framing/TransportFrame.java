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
package org.apache.qpid.proton.framing;

import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.transport.FrameBody;

public class TransportFrame
{
    private final int _channel;
    private final FrameBody _body;
    private final Binary _payload;


    public TransportFrame(final int channel,
                          final FrameBody body,
                          final Binary payload)
    {
        _payload = payload;
        _body = body;
        _channel = channel;
    }

    public int getChannel()
    {
        return _channel;
    }

    public FrameBody getBody()
    {
        return _body;
    }

    public Binary getPayload()
    {
        return _payload;
    }

    @Override
    public String toString()
    {
        StringBuilder builder = new StringBuilder();
        builder.append("TransportFrame{ _channel=").append(_channel).append(", _body=").append(_body).append("}");
        return builder.toString();
    }

}
