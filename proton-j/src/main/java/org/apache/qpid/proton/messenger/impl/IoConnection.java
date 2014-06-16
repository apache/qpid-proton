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

import java.io.IOException;
import java.net.InetSocketAddress;
import java.nio.ByteBuffer;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.SocketChannel;

import org.apache.qpid.proton.messenger.MessengerException;
import org.apache.qpid.proton.messenger.Selectable;

class IoConnection
{
    private Selector _selector;

    private SelectionKey _key;

    private SocketChannel _channel;

    IoConnection(Selector selector, String host, int port)
    {
        try
        {
            _selector = selector;
            _channel = SocketChannel.open();
            _channel.connect(new InetSocketAddress(host, port));
            configureSocket(_channel);
        }
        catch (Exception e)
        {
            e.printStackTrace();
            throw new MessengerException(String.format("Error connecting to %s:%s", host, port), e);
        }
    }

    IoConnection(Selector selector, SocketChannel channel)
    {
        try
        {
            _selector = selector;
            _channel = channel;
            configureSocket(_channel);
        }
        catch (Exception e)
        {
            throw new MessengerException(String.format("Error configuring socket channel"), e);
        }
    }

    void configureSocket(SocketChannel channel)
    {
        try
        {
            channel.configureBlocking(false);
            channel.socket().setTcpNoDelay(true);
        }
        catch (Exception e)
        {
            throw new MessengerException(String.format("Error configuring socket channel"), e);
        }
    }

    void setSelectable(Selectable selectable)
    {
        try
        {
            _key = _channel.register(_selector, SelectionKey.OP_READ | SelectionKey.OP_WRITE);
            _key.attach(selectable);
        }
        catch (Exception e)
        {
            throw new MessengerException(String.format("Error registering socket channel"), e);
        }
    }

    public int read(ByteBuffer buf) throws IOException
    {
        return _channel.read(buf);
    }

    public void registerForReadEvents(boolean b)
    {
        if (!_channel.isOpen())
        {
            return;
        }

        int interest = _key.interestOps();
        if (b)
        {
            interest |= SelectionKey.OP_READ;
        }
        else
        {
            interest &= ~SelectionKey.OP_READ;
        }
        _key.interestOps(interest);
    }

    public int write(ByteBuffer buf) throws IOException
    {
        return _channel.write(buf);
    }

    public void registerForWriteEvents(boolean b)
    {
        if (!_channel.isOpen())
        {
            return;
        }

        int interest = _key.interestOps();
        if (b)
        {
            interest |= SelectionKey.OP_WRITE;
        }
        else
        {
            interest &= ~SelectionKey.OP_WRITE;
        }
        _key.interestOps(interest);
    }

    public void close() throws IOException
    {
        _channel.close();
    }

    public boolean isClosed()
    {
        return !_channel.isOpen();
    }
}