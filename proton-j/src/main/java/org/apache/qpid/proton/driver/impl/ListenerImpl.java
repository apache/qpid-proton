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
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.apache.qpid.proton.driver.Connector;
import org.apache.qpid.proton.driver.Listener;

class ListenerImpl<C> implements Listener<C>
{
    private C _context;
    private final ServerSocketChannel _channel;
    private final DriverImpl _driver;
    private final Logger _logger = Logger.getLogger("proton.driver");
    private boolean _selected = false;

    ListenerImpl(DriverImpl driver, ServerSocketChannel c, C context)
    {
        _driver = driver;
        _channel = c;
        _context = context;
    }

    void selected()
    {
        if (!_selected) {
            _selected = true;
            _driver.selectListener(this);
        }
    }

    void unselected()
    {
        _selected = false;
    }

    public Connector<C> accept()
    {
        try
        {
            SocketChannel c = _channel.accept();
            if(c != null)
            {
                c.configureBlocking(false);
                return _driver.createServerConnector(c, null, this);
            }
        }
        catch (IOException e)
        {
            _logger.log(Level.SEVERE, "Exception when accepting connection",e);
        }
        return null;  //TODO - we should probably throw an exception instead of returning null?
    }

    public C getContext()
    {
        return _context;
    }

    public void setContext(C context)
    {
        _context = context;
    }

    public void close() throws IOException
    {
        _channel.socket().close();
    }
}
