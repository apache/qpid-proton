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
import java.net.ServerSocket;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;

import org.apache.qpid.proton.messenger.Listener;

class SocketListener
{
    private Selector _selector;

    private SelectionKey _key;

    private ServerSocketChannel _serverSocketChannel;

    SocketListener(Selector selector, Listener listener, String host, int port) throws IOException
    {
        _selector = selector;
        _serverSocketChannel = ServerSocketChannel.open();
        ServerSocket serverSocket = _serverSocketChannel.socket();
        serverSocket.bind(new InetSocketAddress(host, port));
        _serverSocketChannel.configureBlocking(false);
        _key = _serverSocketChannel.register(selector, SelectionKey.OP_ACCEPT);
        _key.attach(listener);
    }

    public void close() throws IOException
    {
        _serverSocketChannel.close();
    }

    IoConnection accept() throws IOException
    {
        SocketChannel channel = _serverSocketChannel.accept();
        return new IoConnection(_selector, channel);
    }

    @Override
    public String toString()
    {
        return _serverSocketChannel.socket().toString();
    }
}