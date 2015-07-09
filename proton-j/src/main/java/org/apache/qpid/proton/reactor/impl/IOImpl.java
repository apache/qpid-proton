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

package org.apache.qpid.proton.reactor.impl;

import java.io.IOException;
import java.nio.channels.Pipe;
import java.nio.channels.Selector;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;

public class IOImpl implements IO {

    @Override
    public Pipe pipe() throws IOException {
        return Pipe.open();
    }

    @Override
    public Selector selector() throws IOException {
        return Selector.open();
    }

    @Override
    public ServerSocketChannel serverSocketChannel() throws IOException {
        return ServerSocketChannel.open();
    }

    @Override
    public SocketChannel socketChannel() throws IOException {
        return SocketChannel.open();
    }

}
