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

import static org.apache.qpid.proton.engine.impl.ByteBufferUtils.*;

import java.nio.ByteBuffer;

class TransportOutputAdaptor implements TransportOutput
{
    private TransportOutputWriter _transportOutputWriter;

    private final ByteBuffer _outputBuffer;

    private ByteBuffer _readOnlyOutputBufferView;

    TransportOutputAdaptor(TransportOutputWriter transportOutputWriter, int maxFrameSize)
    {
        _transportOutputWriter = transportOutputWriter;
        _outputBuffer = newWriteableBuffer(maxFrameSize);
    }

    @Override
    public ByteBuffer getOutputBuffer()
    {
        _transportOutputWriter.writeInto(_outputBuffer);
        _outputBuffer.flip();

        _readOnlyOutputBufferView = _outputBuffer.asReadOnlyBuffer();
        return _readOnlyOutputBufferView;
    }

    @Override
    public void outputConsumed()
    {
        if(_readOnlyOutputBufferView == null)
        {
            throw new IllegalStateException("Illegal invocation with previously calling getOutputBuffer");
        }

        _outputBuffer.position(_readOnlyOutputBufferView.position());
        _readOnlyOutputBufferView = null;
        _outputBuffer.compact();
    }

}
