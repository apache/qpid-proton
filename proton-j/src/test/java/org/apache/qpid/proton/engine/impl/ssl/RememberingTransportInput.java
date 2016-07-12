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
    private ByteBuffer _buffer;
    private int _processCount = 0;
    private Integer _zeroCapacityAtCount = null;
    private int _capacityCount = 0;

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
        initIntermediateBuffer();

        _capacityCount++;
        if(_zeroCapacityAtCount != null && _capacityCount >= _zeroCapacityAtCount) {
            return 0;
        }

        return _buffer.remaining();
    }

    @Override
    public int position()
    {
        initIntermediateBuffer();
        return _buffer.position();
    }

    @Override
    public ByteBuffer tail()
    {
        initIntermediateBuffer();
        return _buffer;
    }

    @Override
    public void process() throws TransportException
    {
        _processCount++;

        initIntermediateBuffer();

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

    /**
     * If called before the object is otherwise used, the intermediate input buffer will be
     * initiated to the given size. If called after use, an ISE will be thrown.
     *
     * @param inputBufferSize size of the intermediate input buffer
     * @throws IllegalStateException if the buffer was already initialised
     */
    public void setInputBufferSize(int inputBufferSize) throws IllegalStateException
    {
        if (_buffer != null)
        {
            throw new IllegalStateException("Intermediate input buffer already initialised");
        }

        _inputBufferSize = inputBufferSize;
    }

    private void initIntermediateBuffer()
    {
        if (_buffer == null)
        {
            _buffer = ByteBuffer.allocate(_inputBufferSize);
        }
    }

    int getProcessCount()
    {
        return _processCount;
    }

    int getCapacityCount()
    {
        return _capacityCount;
    }

    /**
     * Sets a point at which calls to capacity will return 0 regardless of the actual buffer state.
     *
     * @param zeroCapacityAtCount number of calls to capacity at which zero starts being returned.
     */
    void setZeroCapacityAtCount(Integer zeroCapacityAtCount)
    {
        if(zeroCapacityAtCount != null && zeroCapacityAtCount < 1) {
            throw new IllegalArgumentException("Value must be null, or at least 1");
        }
        _zeroCapacityAtCount = zeroCapacityAtCount;
    }
}
