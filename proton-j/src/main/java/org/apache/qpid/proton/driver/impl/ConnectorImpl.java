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

import org.apache.qpid.proton.Proton;
import org.apache.qpid.proton.driver.Connector;
import org.apache.qpid.proton.driver.Listener;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Sasl;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.engine.TransportException;
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
    private Transport _transport = Proton.transport();
    private SelectionKey _key;
    private ConnectorState _state = UNINITIALIZED;

    private boolean _inputDone = false;
    private boolean _outputDone = false;
    private boolean _closed = false;

    private boolean _selected = false;
    private boolean _readAllowed = false;

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
        if (!_selected) {
            _selected = true;
            _driver.selectConnector(this);
            _readAllowed = true;
        }
    }

    void unselected()
    {
        _selected = false;
    }

    public boolean process() throws IOException
    {
        if (isClosed() || !_channel.finishConnect()) return false;

        boolean processed = false;
        if (!_inputDone)
        {
            if (read()) {
                processed = true;
            }
        }

        if (!_outputDone)
        {
            if (write()) {
                processed = true;
            }
        }

        if (_outputDone && _inputDone)
        {
            close();
        }

        return processed;
    }

    private boolean read() throws IOException
    {
        if (!_readAllowed) return false;
        _readAllowed = false;
        boolean processed = false;

        int interest = _key.interestOps();
        int capacity = _transport.capacity();
        if (capacity == Transport.END_OF_STREAM)
        {
            _inputDone = true;
        }
        else
        {
            ByteBuffer tail = _transport.tail();
            int bytesRead = _channel.read(tail);
            if (bytesRead < 0) {
                _transport.close_tail();
                _inputDone = true;
            } else if (bytesRead > 0) {
                try {
                    _transport.process();
                } catch (TransportException e) {
                    _logger.log(Level.SEVERE, this + " error processing input", e);
                }
                processed = true;
            }
        }

        capacity = _transport.capacity();
        if (capacity > 0) {
            interest |= SelectionKey.OP_READ;
        } else {
            interest &= ~SelectionKey.OP_READ;
            if (capacity < 0) {
                _inputDone = true;
            }
        }
        _key.interestOps(interest);

        return processed;
    }

    private boolean write() throws IOException
    {
        boolean processed = false;

        int interest = _key.interestOps();
        boolean writeBlocked = false;

        try {
            while (_transport.pending() > 0 && !writeBlocked)
            {
                ByteBuffer head = _transport.head();
                int wrote = _channel.write(head);
                if (wrote > 0) {
                    processed = true;
                    _transport.pop(wrote);
                } else {
                    writeBlocked = true;
                }
            }

            int pending = _transport.pending();
            if (pending > 0) {
                interest |= SelectionKey.OP_WRITE;
            } else {
                interest &= ~SelectionKey.OP_WRITE;
                if (pending < 0) {
                    _outputDone = true;
                }
            }
        } catch (TransportException e) {
            _logger.log(Level.SEVERE, this + " error", e);
            interest &= ~SelectionKey.OP_WRITE;
            _inputDone = true;
            _outputDone = true;
        }

        _key.interestOps(interest);

        return processed;
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
        _transport.bind(_connection);
    }

    public Transport getTransport()
    {
        return _transport;
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
                _channel.close();
            }
            catch (IOException e)
            {
                _logger.log(Level.SEVERE, "Exception when closing connection",e);
            }
            finally
            {
                _closed = true;
                selected();
            }
        }
    }

    public boolean isClosed()
    {
        return _closed;
    }

    public void destroy()
    {
        close(); // close if not closed already
        _driver.removeConnector(this);
    }

    @Override
    public String toString()
    {
        StringBuilder builder = new StringBuilder();
        builder.append("ConnectorImpl [_channel=").append(_channel).append("]");
        return builder.toString();
    }
}
