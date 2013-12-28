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
package org.apache.qpid.proton.driver.impl;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.nio.channels.ClosedChannelException;
import java.nio.channels.SelectableChannel;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.util.ArrayDeque;
import java.util.Collection;
import java.util.Collections;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.Queue;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.apache.qpid.proton.driver.Connector;
import org.apache.qpid.proton.driver.Driver;
import org.apache.qpid.proton.driver.Listener;

public class DriverImpl implements Driver
{
    private Selector _selector;
    private Collection<Listener> _listeners = new LinkedList();
    private Collection<Connector> _connectors = new LinkedList();
    private Logger _logger = Logger.getLogger("proton.driver");
    private Object _wakeupLock = new Object();
    private boolean _woken = false;
    private Queue<ConnectorImpl> _selectedConnectors = new ArrayDeque<ConnectorImpl>();
    private Queue<ListenerImpl> _selectedListeners = new ArrayDeque<ListenerImpl>();

    DriverImpl() throws IOException
    {
        _selector = Selector.open();
    }

    public void wakeup()
    {
        synchronized (_wakeupLock) {
            _woken = true;
        }
        _selector.wakeup();
    }

    public boolean doWait(long timeout)
    {
        try
        {
            boolean woken;
            synchronized (_wakeupLock) {
                woken = _woken;
            }

            if (woken || timeout == 0) {
                _selector.selectNow();
            } else if (timeout < 0) {
                _selector.select();
            } else {
                _selector.select(timeout);
            }

            synchronized (_wakeupLock) {
                woken = woken || _woken;
                _woken = false;
            }

            for (SelectionKey key : _selector.selectedKeys()) {
                if (key.isAcceptable()) {
                    ListenerImpl l = (ListenerImpl) key.attachment();
                    l.selected();
                } else {
                    ConnectorImpl c = (ConnectorImpl) key.attachment();
                    c.selected();
                }
            }

            _selector.selectedKeys().clear();

            return woken;
        }
        catch (IOException e)
        {
            _logger.log(Level.SEVERE, "Exception when waiting for IO Event",e);
            throw new RuntimeException(e);
        }
    }

    void selectListener(ListenerImpl l)
    {
        _selectedListeners.add(l);
    }

    public Listener listener()
    {
        ListenerImpl listener = _selectedListeners.poll();
        if (listener != null) {
            listener.unselected();
        }

        return listener;
    }

    void selectConnector(ConnectorImpl c)
    {
        _selectedConnectors.add(c);
    }

    public Connector connector()
    {
        ConnectorImpl connector = _selectedConnectors.poll();
        if (connector != null) {
            connector.unselected();
        }
        return connector;
    }

    public void destroy()
    {
        try
        {
            _selector.close();
        }
        catch (IOException e)
        {
            _logger.log(Level.SEVERE, "Exception when closing selector",e);
            throw new RuntimeException(e);
        }
        _listeners.clear();
        _connectors.clear();
    }

    public <C> Listener<C> createListener(String host, int port, C context)
    {
        try
        {
            ServerSocketChannel serverSocketChannel = ServerSocketChannel.open();
            ServerSocket serverSocket = serverSocketChannel.socket();
            serverSocket.bind(new InetSocketAddress(host, port));
            serverSocketChannel.configureBlocking(false);
            Listener<C> listener = createListener(serverSocketChannel, context);
            _logger.fine("Created listener on " + host + ":" + port + ": " + context);

            return listener;
        }
        catch (ClosedChannelException e)
        {
            e.printStackTrace();  // TODO - Implement
        }
        catch (IOException e)
        {
            e.printStackTrace();  // TODO - Implement
        }
        return null;
    }

    public <C> Listener<C> createListener(ServerSocketChannel c, C context)
    {
        Listener<C> l = new ListenerImpl<C>(this, c, context);
        SelectionKey key = registerInterest(c,SelectionKey.OP_ACCEPT);
        key.attach(l);
        _listeners.add(l);
        return l;
    }

    public <C> Connector<C> createConnector(String host, int port, C context)
    {
        try
        {
            SocketChannel channel = SocketChannel.open();
            channel.configureBlocking(false);
            // Disable the Nagle algorithm on TCP connections.
            channel.socket().setTcpNoDelay(true);
            channel.connect(new InetSocketAddress(host, port));
            return createConnector(channel, context);
        }
        catch (IOException e)
        {
            // TODO Auto-generated catch block
            e.printStackTrace();
            throw new RuntimeException(e);
        }
    }

    public <C> Connector<C> createConnector(SelectableChannel c, C context)
    {
        SelectionKey key = registerInterest(c, SelectionKey.OP_READ | SelectionKey.OP_WRITE);
        Connector<C> co = new ConnectorImpl<C>(this, null, (SocketChannel)c, context, key);
        key.attach(co);
        _connectors.add(co);
        return co;
    }

    public <C> void removeConnector(Connector<C> c)
    {
        _connectors.remove(c);
    }

    public Iterable<Listener> listeners()
    {
        return _listeners;
    }

    public Iterable<Connector> connectors()
    {
        return _connectors;
    }

    protected <C> Connector<C> createServerConnector(SelectableChannel c, C context, Listener<C> l)
    {
        SelectionKey key = registerInterest(c, SelectionKey.OP_READ | SelectionKey.OP_WRITE);
        Connector<C> co = new ConnectorImpl<C>(this, l, (SocketChannel)c, context, key);
        key.attach(co);
        _connectors.add(co);
        return co;
    }

    private <C> SelectionKey registerInterest(SelectableChannel c, int opKeys)
    {
        try
        {
            return c.register(_selector, opKeys);
        }
        catch (ClosedChannelException e)
        {
            e.printStackTrace();  // TODO - Implement
            throw new RuntimeException(e);
        }
    }
}
