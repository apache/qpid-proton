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
package org.apache.qpid.proton.engine.impl.ssl;

import java.nio.ByteBuffer;

import org.apache.qpid.proton.engine.impl.TransportOutput;

public class CannedTransportOutput implements TransportOutput
{

    private ByteBuffer _cannedOutput;
    private ByteBuffer _head;
    private int _popped;

    public CannedTransportOutput()
    {
    }

    public CannedTransportOutput(String output)
    {
        setOutput(output);
    }

    public void setOutput(String output)
    {
        _cannedOutput = ByteBuffer.wrap(output.getBytes());
        _head = _cannedOutput.asReadOnlyBuffer();
        _popped = 0;
    }

    @Override
    public int pending()
    {
        return _head.remaining();
    }

    @Override
    public ByteBuffer head()
    {
        return _head;
    }

    @Override
    public void pop(int bytes)
    {
        _popped += bytes;
        _head.position(_popped);
    }

    @Override
    public void close_head()
    {
        // do nothing
    }


}
