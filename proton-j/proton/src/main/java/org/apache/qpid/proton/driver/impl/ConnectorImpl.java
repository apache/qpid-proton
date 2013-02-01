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

import static org.apache.qpid.proton.driver.impl.ConnectorImpl.ConnectorState.UNINITIALIZED;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.SelectionKey;
import java.nio.channels.SocketChannel;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.apache.qpid.proton.driver.Connector;
import org.apache.qpid.proton.driver.Listener;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Sasl;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.engine.impl.TransportFactory;

class ConnectorImpl<C> implements Connector<C>
{
    private static int DEFAULT_BUFFER_SIZE = 64 * 1024;
    private static int readBufferSize = Integer.getInteger
        ("pn.receive_buffer_size", DEFAULT_BUFFER_SIZE);
    private static int writeBufferSize = Integer.getInteger
        ("pn.send_buffer_size", DEFAULT_BUFFER_SIZE);

    enum ConnectorState {UNINITIALIZED, OPENED, EOS, CLOSED};

    private final DriverImpl _driver;
    private final Listener<C> _listener;
    private final SocketChannel _channel;
    private final Logger _logger = Logger.getLogger("proton.driver");
    private C _context;

    private Connection _connection;
    private Transport _transport = null;
    private SelectionKey _key;
    private ConnectorState _state = UNINITIALIZED;

    private ByteBuffer _readBuffer = ByteBuffer.allocate(readBufferSize);
    private ByteBuffer _writeBuffer = ByteBuffer.allocate(writeBufferSize);
    private boolean _readPending = true;

    ConnectorImpl(DriverImpl driver, Listener<C> listener, SocketChannel c, C context, SelectionKey key)
    {
        _driver = driver;
        _listener = listener;
        _channel = c;
        _context = context;
        _key = key;
    }

    void selected()
    {
        _readPending = true;
    }

    public void process() throws IOException
    {
        if (_channel.isOpen() && _channel.finishConnect())
        {
            if (_readPending)
            {
                read();
                _readPending = false;
                if (isClosed()) return;
            }
            else
            {
                processInput();
            }
            write();
        }
    }

    private void read() throws IOException
    {
        int bytesRead = 0;
        while ((bytesRead = _channel.read(_readBuffer)) > 0)
        {
            processInput();
        }
        if (bytesRead == -1) {
            close();
        }
    }

    private int processInput() throws IOException
    {
        _readBuffer.flip();
        int total = 0;
        while (_readBuffer.hasRemaining())
        {
            int consumed = _transport.input(_readBuffer.array(), _readBuffer.position(), _readBuffer.remaining());
            if (consumed == Transport.END_OF_STREAM)
            {
                continue;
            }
            else if (consumed == 0)
            {
                break;
            }
            _readBuffer.position(_readBuffer.position() + consumed);
            if (_logger.isLoggable(Level.FINE))
            {
                _logger.log(Level.FINE, "consumed " + consumed + " bytes, " + _readBuffer.remaining() + " available");
            }
            total += consumed;
        }
        _readBuffer.compact();
        return total;
    }

    private void write() throws IOException
    {
        int interest = _key.interestOps();
        boolean empty = _writeBuffer.position() == 0;
        boolean done = false;
        while (!done)
        {
            int produced = _transport.output(_writeBuffer.array(), _writeBuffer.position(), _writeBuffer.remaining());
            _writeBuffer.position(_writeBuffer.position() + produced);
            _writeBuffer.flip();
            int wrote = _channel.write(_writeBuffer);
            if (_logger.isLoggable(Level.FINE))
            {
                _logger.log(Level.FINE, "wrote " + wrote + " bytes, " + _writeBuffer.remaining() + " remaining");
            }
            if (_writeBuffer.hasRemaining())
            {
                //weren't able to write all available data, ask to be notfied when we can write again
                _writeBuffer.compact();
                interest |= SelectionKey.OP_WRITE;
                done = true;
            }
            else
            {
                //we are done if buffer was empty to begin with and we did not produce anything
                _writeBuffer.clear();
                interest &= ~SelectionKey.OP_WRITE;
                done = empty && produced == 0;
                empty = true;
            }
        }
        _key.interestOps(interest);
    }

    public Listener<C> listener()
    {
        return _listener;
    }

    public Sasl sasl()
    {
        if (_transport != null)
        {
            return _transport.sasl();
        }
        else
        {
            return null;
        }
    }

    public Connection getConnection()
    {
        return _connection;
    }

    public void setConnection(Connection connection)
    {
        _connection = connection;
        _transport = TransportFactory.getDefaultTransportFactory().transport(_connection);
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
        if (!isClosed())
        {
            try
            {
                write();
                _channel.close();
            }
            catch (IOException e)
            {
                _logger.log(Level.SEVERE, "Exception when closing connection",e);
            }
        }
    }

    public boolean isClosed()
    {
        boolean result = !(_channel.isOpen() && _channel.isConnected());
        return result;
    }

    public void destroy()
    {
        close(); // close if not closed already
        _driver.removeConnector(this);
    }
}
