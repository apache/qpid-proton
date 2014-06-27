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

import org.apache.qpid.proton.engine.Transport;

class TransportOutputAdaptor implements TransportOutput
{
    private static final ByteBuffer _emptyHead = newReadableBuffer(0).asReadOnlyBuffer();

    private final TransportOutputWriter _transportOutputWriter;
    private final int _maxFrameSize;
    private final ByteBuffer _scratchBuffer;

    private ByteBuffer _outputBuffer = null;
    private ByteBuffer _head = null;
    private boolean _output_done = false;
    private boolean _head_closed = false;

    TransportOutputAdaptor(TransportOutputWriter transportOutputWriter, int maxFrameSize)
    {
        _transportOutputWriter = transportOutputWriter;
        _maxFrameSize = maxFrameSize > 0 ? maxFrameSize : 4*1024;
        _scratchBuffer = newWriteableBuffer(Math.min(512, _maxFrameSize));
    }

    @Override
    public int pending()
    {
        if (_head_closed) {
            return Transport.END_OF_STREAM;
        }

        try_fill_buffer();

        if (_outputBuffer != null && _outputBuffer.position() == 0) {
            release_buffers();
        }

        if (_outputBuffer == null) {
            if (_output_done) {
                return Transport.END_OF_STREAM;
            } else {
                return 0;
            }
        } else {
            return _outputBuffer.position();
        }
    }

    @Override
    public ByteBuffer head()
    {
        pending();
        return _head != null ? _head : _emptyHead;
    }

    @Override
    public void pop(int bytes)
    {
        if (_outputBuffer != null) {
            _outputBuffer.flip();
            _outputBuffer.position(bytes);
            _outputBuffer.compact();
            _head.position(0);
            _head.limit(_outputBuffer.position());
            if (_outputBuffer.position() == 0) {
                release_buffers();
            }
        }
    }

    @Override
    public void close_head()
    {
        _head_closed = true;
        _transportOutputWriter.closed();
        release_buffers();
    }

    private void init_buffers() {
        _outputBuffer = newWriteableBuffer(_maxFrameSize);
        _head = _outputBuffer.asReadOnlyBuffer();
        _head.limit(0);
    }

    private void release_buffers() {
        _head = null;
        _outputBuffer = null;
    }

    private void try_fill_buffer() {
        boolean done = false;
        while (!done) {
            reset_scratch_buffer();
            _output_done |= _transportOutputWriter.writeInto(_scratchBuffer);
            done = _scratchBuffer.position() < _scratchBuffer.capacity();
            if (_scratchBuffer.position() > 0) {
                copy_scratch_to_output();
            }
        }
    }

    private void reset_scratch_buffer() {
        _scratchBuffer.clear();
        if (_outputBuffer != null) {
            _scratchBuffer.limit(Math.min(_scratchBuffer.capacity(), _outputBuffer.capacity() - _outputBuffer.position()));
        }
    }

    private void copy_scratch_to_output() {
        if (_outputBuffer == null) {
            init_buffers();
        }
        _scratchBuffer.flip();
        _outputBuffer.put(_scratchBuffer);
        _head.limit(_outputBuffer.position());
    }
}
