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
package org.apache.qpid.proton.messenger.impl;

import java.util.concurrent.atomic.AtomicBoolean;

import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.messenger.Selectable;

public class SelectableImpl implements Selectable
{
    private final AtomicBoolean _connected = new AtomicBoolean(false);

    private final AtomicBoolean _completed = new AtomicBoolean(false);

    private final AtomicBoolean _closed = new AtomicBoolean(false);

    private Object _ctx;

    private Transport _transport;

    private Connection _connection;

    private String _host = null;

    private int _port = -1;

    private IoConnection _networkConnection;

    SelectableImpl(String host, int port)
    {
        _host = host;
        _port = port;
    }

    SelectableImpl()
    {
    }

    Object getContext()
    {
        return _ctx;
    }

    void setContext(Object ctx)
    {
        _ctx = ctx;
    }

    @Override
    public Transport getTransport()
    {
        return _transport;
    }

    void setTransport(Transport t)
    {
        _transport = t;
    }

    Connection getConnection()
    {
        return _connection;
    }

    void setConnection(Connection c)
    {
        _connection = c;
    }

    void markClosed()
    {
        _closed.set(true);
    }

    @Override
    public String getHost()
    {
        return _host;
    }

    @Override
    public int getPort()
    {
        return _port;
    }

    @Override
    public boolean isClosed()
    {
        return _closed.get();
    }

    @Override
    public void markCompleted()
    {
        _completed.set(true);
    }

    @Override
    public boolean isCompleted()
    {
        return _completed.get();
    }

    @Override
    public boolean isConnected()
    {
        return _connected.get();
    }

    @Override
    public void markConnected()
    {
        _connected.set(true);
    }

    // Used in non passive mode.

    void setNetworkConnection(IoConnection con)
    {
        _networkConnection = con;
    }

    IoConnection getNetworkConnection()
    {
        return _networkConnection;
    }
}