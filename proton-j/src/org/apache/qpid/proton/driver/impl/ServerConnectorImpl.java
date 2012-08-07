/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package org.apache.qpid.proton.driver.impl;

import static org.apache.qpid.proton.driver.impl.ServerConnectorImpl.ConnectorState.UNINITIALIZED;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.SelectionKey;
import java.nio.channels.SocketChannel;

import org.apache.qpid.proton.driver.Connector;
import org.apache.qpid.proton.driver.Listener;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Sasl;
import org.apache.qpid.proton.logging.LogHandler;

class ServerConnectorImpl<C> implements Connector<C>
{
    public static int END_OF_STREAM = -1;
    private static int DEFAULT_BUFFER_SIZE = 64 * 1024;
    private static int readBufferSize = Integer.getInteger
        ("pn.receive_buffer_size", DEFAULT_BUFFER_SIZE);
    private static int writeBufferSize = Integer.getInteger
        ("pn.send_buffer_size", DEFAULT_BUFFER_SIZE);

    enum ConnectorState {UNINITIALIZED, OPENED, EOS, CLOSED};

    private final Sasl _sasl;
    private final DriverImpl _driver;
    private final Listener<C> _listener;
    private final SocketChannel _channel;
    private final LogHandler _logger;
    private C _context;

    private Connection _connection;
    private SelectionKey _key;
    private ConnectorState _state = UNINITIALIZED;

    private ByteBuffer _readBuffer = ByteBuffer.allocate(readBufferSize);
    private int _bytesNotRead = 0;

    private int _bytesNotWritten = 0;
    private ByteBuffer _writeBuffer = ByteBuffer.allocate(writeBufferSize);

    ServerConnectorImpl(DriverImpl driver, Listener<C> listener, Sasl sasl, SocketChannel c, C context, SelectionKey key)
    {
        _driver = driver;
        _listener = listener;
        _channel = c;
        _sasl = sasl;
        _logger = driver.getLogHandler();
        _context = context;
        _key = key;
    }

    public void process()
    {
        if (!_channel.isOpen())
        {
            _state = ConnectorState.CLOSED;
            return;
        }

        if (_key.isReadable())
        {
            read();
        }

        if (_key.isWritable())
        {
            write();
        }
    }

    void read()
    {
        try
        {
            int  bytesRead = _channel.read(_readBuffer);
            int consumed = 0;
            while (bytesRead > 0)
            {
                consumed = processInput(_readBuffer.array(), 0, bytesRead + _bytesNotRead);
                if (consumed < bytesRead)
                {
                    _readBuffer.compact();
                    _bytesNotRead = bytesRead - consumed;
                }
                else
                {
                    _readBuffer.rewind();
                    _bytesNotRead = 0;
                }
                bytesRead = _channel.read(_readBuffer);
            }
            if (bytesRead == -1)
            {
                _state = ConnectorState.EOS;
            }
        }
        catch (IOException e)
        {
            e.printStackTrace();
        }
    }

    void write()
    {
        try
        {
            processOutput();
            if (_bytesNotWritten > 0)
            {
                _writeBuffer.limit(_bytesNotWritten);
                int written = _channel.write(_writeBuffer);
                if (_writeBuffer.hasRemaining())
                {
                    _writeBuffer.compact();
                    _bytesNotWritten = _bytesNotWritten - written;
                }
                else
                {
                    _writeBuffer.clear();
                    _bytesNotWritten = 0;
                }
            }
        }
        catch (IOException e)
        {
            e.printStackTrace();
        }
    }

    int processInput(byte[] bytes, int offset, int size)
    {
        int read = 0;
        while (read < size)
        {
            switch (_state)
            {
            case UNINITIALIZED:
                read += readSasl(bytes, offset, size);
                break;
            case OPENED:
                read += readAMQPCommands(bytes, offset, size);
                break;
            case EOS:
            case CLOSED:
                break;
            }
        }
        return read;
    }

    void processOutput()
    {
        switch (_state)
        {
        case UNINITIALIZED:
            writeSasl();
            break;
        case OPENED:
            writeAMQPCommands();
            break;
        case EOS:
            writeAMQPCommands();
        case CLOSED:  // not a valid option
            //TODO
            break;
        }
    }

    int readAMQPCommands(byte[] bytes, int offset, int size)
    {
        int consumed = _connection.transport().input(bytes, offset, size);
        if (consumed == END_OF_STREAM)
        {
            return size;
        }
        else
        {
            return consumed;
        }
    }

    void writeAMQPCommands()
    {
        int size = _writeBuffer.array().length - _bytesNotWritten;
        _bytesNotWritten += _connection.transport().output(_writeBuffer.array(),
                _bytesNotWritten, size);
    }

    int readSasl(byte[] bytes, int offset, int size)
    {
        int consumed = _sasl.input(bytes, offset, size);
        if (consumed == END_OF_STREAM)
        {
            return size;
        }
        else
        {
            return consumed;
        }
    }

    void writeSasl()
    {
        int size = _writeBuffer.array().length - _bytesNotWritten;
        _bytesNotWritten += _sasl.output(_writeBuffer.array(),
                _bytesNotWritten, size);
    }

    public Listener<C> listener()
    {
        return _listener;
    }

    public Sasl sasl()
    {
        return _sasl;
    }

    public Connection getConnection()
    {
        return _connection;
    }

    public void setConnection(Connection connection)
    {
        // write any remaining data on to the wire.
        writeSasl();
        _connection = connection;
        // write initial data
        int size = _writeBuffer.array().length - _bytesNotWritten;
        _bytesNotWritten += _connection.transport().output(_writeBuffer.array(),
                _bytesNotWritten, size);
        setState(ConnectorState.OPENED);
    }

    public C getContext()
    {
        return _context;
    }

    public void setContext(C context)
    {
        _context = context;
    }

    public void close()
    {
        if (_state == ConnectorState.CLOSED)
        {
            return;
        }

        try
        {
            // If the connection was closed due to authentication error
            // then there might be data available to write on to the wire.
            writeSasl();
            writeAMQPCommands(); // write any closing commands
            _channel.close();
            _state = ConnectorState.CLOSED;
        }
        catch (IOException e)
        {
            e.printStackTrace();
        }
    }

    public boolean isClosed()
    {
        return _state == ConnectorState.EOS || _state == ConnectorState.CLOSED;
    }

    public void destroy()
    {
        close(); // close if not closed already
    }

    private void setState(ConnectorState newState)
    {
        _state = newState;
    }
}
