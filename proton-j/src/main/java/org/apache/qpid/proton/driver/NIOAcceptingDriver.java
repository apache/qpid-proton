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

import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.engine.impl.ConnectionImpl;
import org.apache.qpid.proton.engine.impl.TransportImpl;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.SocketAddress;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

public class NIOAcceptingDriver implements Runnable
{
    private final Selector _selector;
    
    private final ApplicationFactory _applicationFactory;


    public NIOAcceptingDriver(final SocketAddress bindAddress, ApplicationFactory applicationFactory)
    {
        try
        {
            _applicationFactory = applicationFactory;
            _selector = Selector.open();
            
            ServerSocketChannel serverSocketChannel = ServerSocketChannel.open();
            serverSocketChannel.configureBlocking(false);
            ServerSocket serverSocket = serverSocketChannel.socket();
            serverSocket.bind(bindAddress);
            serverSocketChannel.register(_selector, SelectionKey.OP_ACCEPT);
        }
        catch (IOException e)
        {
            e.printStackTrace();  //TODO.
            throw new RuntimeException(e);
        }

    }

    public void run()
    {
        while(true)
        {
            try
            {
                _selector.select();
                Iterator<SelectionKey> selectedIter = _selector.selectedKeys().iterator();
                while(selectedIter.hasNext())                    
                {
                    SelectionKey key = selectedIter.next();
                    selectedIter.remove();
                    if(key.isAcceptable())
                    {
                        ServerSocketChannel channel = (ServerSocketChannel) key.channel();
                        SocketChannel newChan = channel.accept();
                        newChan.configureBlocking(false);
                        ConnectionImpl connection = new ConnectionImpl();
                        Transport amqpTransport = new TransportImpl();
                        amqpTransport.bind(connection);
                        Map<byte[], Transport> transportMap = new HashMap<byte[], Transport>();
                        transportMap.put(TransportImpl.HEADER, amqpTransport);
                        DelegatingTransport transport = new DelegatingTransport(transportMap, amqpTransport);
                        newChan.register(_selector, SelectionKey.OP_READ,
                                         new ConnectionTransport(connection, transport, newChan,
                                                                 _applicationFactory.createApplication()));

                    }
                    else if(key.isReadable() || key.isWritable())
                    {
                        ConnectionTransport connectionTransport = (ConnectionTransport) key.attachment();

                        connectionTransport.process();

                        if(connectionTransport.isReadClosed())
                        {
                            key.cancel();
                        }
                        else
                        {
                            if(connectionTransport.isWritable())
                            {
                                key.interestOps(key.interestOps() | SelectionKey.OP_WRITE);
                            }
                            else
                            {
                                if((key.interestOps() & SelectionKey.OP_WRITE) != 0)
                                {
                                    key.interestOps(key.interestOps() & ~SelectionKey.OP_WRITE);
                                }
                            }
                        }

                    }
                    else if(key.isConnectable())
                    {
                        System.out.println("connectable");
                    }


                }
            }
            catch (IOException e)
            {
                e.printStackTrace();  //TODO.
            }
        }
    }
    
    public static void main(String[] args)
    {
        SocketAddress bindAddress = new InetSocketAddress(5672);
        NIOAcceptingDriver driver = new NIOAcceptingDriver(bindAddress, new ApplicationFactory()
        {
            public Application createApplication()
            {
                return new NoddyBrokerApplication();
            }
        });
        driver.run();
    }
}
