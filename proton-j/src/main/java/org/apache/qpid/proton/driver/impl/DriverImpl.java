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
import java.util.Collections;
import java.util.Iterator;
import java.util.Set;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.apache.qpid.proton.driver.Connector;
import org.apache.qpid.proton.driver.Driver;
import org.apache.qpid.proton.driver.Listener;
import org.apache.qpid.proton.engine.impl.SaslClientImpl;
import org.apache.qpid.proton.engine.impl.SaslServerImpl;

public class DriverImpl implements Driver
{
    private Selector _selector;
    private Set<SelectionKey> _selectedKeys = Collections.emptySet();
    private Logger _logger = Logger.getLogger("proton.driver");

    public DriverImpl() throws IOException
    {
        _selector = Selector.open();
    }

    public void wakeup()
    {
        _selector.wakeup();
    }

    public void doWait(int timeout)
    {
        try
        {
            _selector.select(timeout);
            _selectedKeys = _selector.selectedKeys();
        }
        catch (IOException e)
        {
            _logger.log(Level.SEVERE, "Exception when waiting for IO Event",e);
            throw new RuntimeException(e);
        }
    }

    @SuppressWarnings("rawtypes")
    public Listener listener()
    {
        Listener listener = null;
        listener = getFirstListener();
        if(listener == null)
        {
            try
            {
                selectNow();
            }
            catch (IOException e)
            {
                _logger.log(Level.SEVERE, "Exception when selecting",e);
                throw new RuntimeException(e);
            }
            listener = getFirstListener();
        }
        return listener;
    }

    private void selectNow() throws IOException
    {
        _selector.selectNow();
        _selectedKeys = _selector.selectedKeys();
    }

    @SuppressWarnings("rawtypes")
    private Listener getFirstListener()
    {
        Iterator<SelectionKey> selectedIter = _selectedKeys.iterator();

        while(selectedIter.hasNext())
        {
            SelectionKey key = selectedIter.next();
            selectedIter.remove();
            if(key.isAcceptable())
            {
                return (Listener) key.attachment();
            }
        }
        return null;
    }

    @SuppressWarnings("rawtypes")
    public Connector connector()
    {
        Connector connector = null;
        connector = getFirstConnector();
        if(connector == null)
        {
            try
            {
                selectNow();
            }
            catch (IOException e)
            {
                _logger.log(Level.SEVERE, "Exception when selecting",e);
                throw new RuntimeException(e);
            }
            connector = getFirstConnector();
        }
        return connector;
    }

    @SuppressWarnings("rawtypes")
    private Connector getFirstConnector()
    {
        Iterator<SelectionKey> selectedIter = _selectedKeys.iterator();

        while(selectedIter.hasNext())
        {
            SelectionKey key = selectedIter.next();
            selectedIter.remove();
            if(key.isReadable() || key.isWritable())
            {
                return (Connector) key.attachment();

            }
        }
        return null;
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
    }

    public <C> Listener<C> createListener(String host, int port, C context)
    {
        try
        {
            ServerSocketChannel serverSocketChannel = ServerSocketChannel.open();
            serverSocketChannel.configureBlocking(false);
            ServerSocket serverSocket = serverSocketChannel.socket();
            serverSocket.bind(new InetSocketAddress(host, port));
            return createListener(serverSocketChannel, context);
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
        return l;
    }

    public <C> Connector<C> createConnector(String host, int port, C context)
    {
        try
        {
            SocketChannel channel = SocketChannel.open();
            channel.configureBlocking(false);
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
        SelectionKey key = registerInterest(c,SelectionKey.OP_READ | SelectionKey.OP_WRITE);
        Connector<C> co = new ConnectorImpl<C>(this, null, new SaslClientImpl(),(SocketChannel)c, context, key);
        key.attach(co);
        return co;
    }

    protected <C> Connector<C> createServerConnector(SelectableChannel c, C context, Listener<C> l)
    {
        SelectionKey key = registerInterest(c,SelectionKey.OP_READ | SelectionKey.OP_WRITE);
        Connector<C> co = new ConnectorImpl<C>(this, l, new SaslServerImpl(),(SocketChannel)c, context, key);
        key.attach(co);
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
