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

package org.apache.qpid.proton.driver;

import org.apache.qpid.proton.engine.impl.ConnectionImpl;
import org.apache.qpid.proton.engine.impl.ConnectionImpl;
import org.apache.qpid.proton.engine.impl.ConnectionImpl;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.ByteChannel;
import java.util.concurrent.atomic.AtomicBoolean;

class ConnectionTransport
{
    public static final int DEFAULT_BUFFER_SIZE = 16 * 1024;
    private final ConnectionImpl _connection;
    private final BytesTransport _transport;
    private final Application _application;
    private final ByteChannel _channel;

    private final byte[] _inputBuffer; 
    private final byte[] _outputBuffer;;
    
    private int _inputHead = 0;
    private int _inputTail = 0;
    
    private int _outputHead = 0;
    private int _outputTail = 0;
    private final int _mask;
    private boolean _readClosed;
    private boolean _writeClosed;
    private boolean _readable;
    private boolean _writable;
    private final AtomicBoolean _state = new AtomicBoolean();


    ConnectionTransport(ConnectionImpl connection, BytesTransport transport, ByteChannel channel,
                        Application application)
    {
        _connection = connection;
        _transport = transport;
        _application = application;
        _channel = channel;
        _inputBuffer = new byte[DEFAULT_BUFFER_SIZE];
        _outputBuffer = new byte[DEFAULT_BUFFER_SIZE];
        _mask = _inputBuffer.length - 1;
    }

    public ConnectionImpl getConnection()
    {
        return _connection;
    }

    public BytesTransport getTransport()
    {
        return _transport;
    }

    public Application getApplication()
    {
        return _application;
    }
    
    public boolean process() throws IOException
    {

        if(_state.compareAndSet(false, true))
        {
            int read;
            // TODO - should reuse the same buffer
            ByteBuffer buf = ByteBuffer.wrap(_inputBuffer);
            while((read=_channel.read(buf))>0)
            {
                _transport.input(_inputBuffer,_inputHead & _mask,(_inputHead & _mask) + read);
                buf.clear();

            }

            if(read == -1)
            {
                _channel.close();
                setReadClosed(true);

            }

            _application.process(_connection);

            int length = _transport.output(_outputBuffer, 0, _outputBuffer.length);

            int offset = 0;
            while(length > 0)
            {
                int written = _channel.write(ByteBuffer.wrap(_outputBuffer,offset,length));
                if(written == -1)
                {
                    setWriteClosed(true);
                    break;
                }
                else
                {
                    length -= written;
                    offset += written;
                }
            }

            _readable = _inputTail - _inputHead < _inputBuffer.length;
            _writable = _outputTail > _outputHead;

            _state.set(false);
            return true;
        }
        else
        {
            return false;
        }

    }

    public void setReadClosed(boolean readClosed)
    {
        _readClosed = readClosed;
    }

    public boolean isReadClosed()
    {
        return _readClosed;
    }

    public void setWriteClosed(boolean writeClosed)
    {
        _writeClosed = writeClosed;
    }

    public boolean isWriteClosed()
    {
        return _writeClosed;
    }

    public boolean isReadable()
    {
        return _readable;
    }

    public boolean isWritable()
    {
        return _writable;
    }
}
