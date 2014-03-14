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

import org.apache.qpid.proton.engine.TransportException;
import org.apache.qpid.proton.engine.impl.TransportInput;

class RememberingTransportInput implements TransportInput
{
    private StringBuilder _receivedInput = new StringBuilder();
    private String _nextError;
    private int _inputBufferSize = 1024;
    private ByteBuffer _buffer = ByteBuffer.allocate(_inputBufferSize);

    String getAcceptedInput()
    {
        return _receivedInput.toString();
    }

    @Override
    public String toString()
    {
        return "[RememberingTransportInput receivedInput (length " + _receivedInput.length() + ") is:" + _receivedInput.toString() + "]";
    }

    @Override
    public int capacity()
    {
        return _buffer.remaining();
    }

    @Override
    public ByteBuffer tail()
    {
        return _buffer;
    }

    @Override
    public void process() throws TransportException
    {
        if(_nextError != null)
        {
            throw new TransportException(_nextError);
        }

        _buffer.flip();
        byte[] receivedInputBuffer = new byte[_buffer.remaining()];
        _buffer.get(receivedInputBuffer);
        _buffer.compact();
        _receivedInput.append(new String(receivedInputBuffer));
    }

    @Override
    public void close_tail()
    {
        // do nothing
    }

    public void rejectNextInput(String nextError)
    {
        _nextError = nextError;
    }

    public void setInputBufferSize(int inputBufferSize)
    {
        _inputBufferSize = inputBufferSize;
    }
}