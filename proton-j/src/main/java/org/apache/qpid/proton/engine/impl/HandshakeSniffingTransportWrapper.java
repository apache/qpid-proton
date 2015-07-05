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

import java.nio.ByteBuffer;

import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.engine.TransportException;
import org.apache.qpid.proton.engine.impl.TransportWrapper;

public abstract class HandshakeSniffingTransportWrapper<T1 extends TransportWrapper, T2 extends TransportWrapper>
    implements TransportWrapper
{

    protected final T1 _wrapper1;
    protected final T2 _wrapper2;

    private boolean _tail_closed = false;
    private boolean _head_closed = false;
    protected TransportWrapper _selectedTransportWrapper;

    private final ByteBuffer _determinationBuffer;

    protected HandshakeSniffingTransportWrapper
        (T1 wrapper1,
         T2 wrapper2)
    {
        _wrapper1 = wrapper1;
        _wrapper2 = wrapper2;
        _determinationBuffer = ByteBuffer.allocate(bufferSize());
    }

    @Override
    public int capacity()
    {
        if (isDeterminationMade())
        {
            return _selectedTransportWrapper.capacity();
        }
        else
        {
            if (_tail_closed) { return Transport.END_OF_STREAM; }
            return _determinationBuffer.remaining();
        }
    }

    @Override
    public int position()
    {
        if (isDeterminationMade())
        {
            return _selectedTransportWrapper.position();
        }
        else
        {
            if (_tail_closed) { return Transport.END_OF_STREAM; }
            return _determinationBuffer.position();
        }
    }

    @Override
    public ByteBuffer tail()
    {
        if (isDeterminationMade())
        {
            return _selectedTransportWrapper.tail();
        }
        else
        {
            return _determinationBuffer;
        }
    }

    protected abstract int bufferSize();

    protected abstract void makeDetermination(byte[] bytes);

    @Override
    public void process() throws TransportException
    {
        if (isDeterminationMade())
        {
            _selectedTransportWrapper.process();
        }
        else if (_determinationBuffer.remaining() == 0)
        {
            _determinationBuffer.flip();
            byte[] bytesInput = new byte[_determinationBuffer.remaining()];
            _determinationBuffer.get(bytesInput);
            makeDetermination(bytesInput);
            _determinationBuffer.rewind();

            // TODO what if the selected transport has insufficient capacity?? Maybe use pour, and then try to finish pouring next time round.
            _selectedTransportWrapper.tail().put(_determinationBuffer);
            _selectedTransportWrapper.process();
        } else if (_tail_closed) {
            throw new TransportException("connection aborted");
        }
    }

    @Override
    public void close_tail()
    {
        try {
            if (isDeterminationMade())
            {
                _selectedTransportWrapper.close_tail();
            }
        } finally {
            _tail_closed = true;
        }
    }

    @Override
    public int pending()
    {
        if (_head_closed) { return Transport.END_OF_STREAM; }

        if (isDeterminationMade()) {
            return _selectedTransportWrapper.pending();
        } else {
            return 0;
        }

    }

    private static final ByteBuffer EMPTY = ByteBuffer.allocate(0);

    @Override
    public ByteBuffer head()
    {
        if (isDeterminationMade()) {
            return _selectedTransportWrapper.head();
        } else {
            return EMPTY;
        }
    }

    @Override
    public void pop(int bytes)
    {
        if (isDeterminationMade()) {
            _selectedTransportWrapper.pop(bytes);
        } else if (bytes > 0) {
            throw new IllegalStateException("no bytes have been read");
        }
    }

    @Override
    public void close_head()
    {
        if (isDeterminationMade()) {
            _selectedTransportWrapper.close_head();
        } else {
            _head_closed = true;
        }
    }

    protected boolean isDeterminationMade()
    {
        return _selectedTransportWrapper != null;
    }

}
