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

import java.nio.channels.SelectableChannel;
import java.nio.channels.ServerSocketChannel;

/**
 * The Driver interface provides an abstraction for an implementation of
 * a driver for the proton engine.
 * A driver is responsible for providing input, output, and tick events,
 * to the bottom half of the engine API. TODO See ::pn_input, ::pn_output, and
 * ::pn_tick.
 * The driver also provides an interface for the application to access,
 * the top half of the API when the state of the engine may have changed
 * due to I/O or timing events. Additionally the driver incorporates the SASL
 * engine as well in order to provide a complete network stack: AMQP over SASL
 * over TCP.
 */

public interface Driver
{
    /**
     * Force wait() to return
     *
     */
    void wakeup();

    /**
     * Wait for an active connector or listener
     *
     * @param timeout maximum time in milliseconds to wait.
     *                0 means infinite wait
     */
    void doWait(int timeout);

    /**
     * Get the next listener with pending data in the driver.
     *
     * @return NULL if no active listener available
     */
    @SuppressWarnings("rawtypes")
    Listener listener();

    /**
     * Get the next active connector in the driver.
     *
     * Returns the next connector with pending inbound data, available capacity
     * for outbound data, or pending tick.
     *
     * @return NULL if no active connector available
     */
    @SuppressWarnings("rawtypes")
    Connector connector();

    /**
     * Destruct the driver and all associated listeners, connectors and other resources.
     */
    void destroy();

    /**
     * Construct a listener for the given address.
     *
     * @param host local host address to listen on
     * @param port local port to listen on
     * @param context application-supplied, can be accessed via
     *                {@link Listener#getContext() getContext()} method on a listener.
     * @return a new listener on the given host:port, NULL if error
     */
    <C> Listener<C> createListener(String host, int port, C context);

    /**
     * Create a listener using the existing channel.
     *
     * @param c   existing SocketChannel for listener to listen on
     * @param context application-supplied, can be accessed via
     *                {@link Listener#getContext() getContext()} method on a listener.
     * @return a new listener on the given channel, NULL if error
     */
    <C> Listener<C> createListener(ServerSocketChannel c, C context);

    /**
     * Construct a connector to the given remote address.
     *
     * @param host remote host to connect to.
     * @param port remote port to connect to.
     * @param context application-supplied, can be accessed via
     *                {@link Connector#getContext() getContext()} method on a listener.
     *
     * @return a new connector to the given remote, or NULL on error.
     */
    <C> Connector<C> createConnector(String host, int port, C context);

    /**
     * Create a connector using the existing file descriptor.
     *
     * @param c   existing SocketChannel for listener to listen on
     * @param context application-supplied, can be accessed via
     *                {@link Connector#getContext() getContext()} method on a listener.
     *
     * @return a new connector to the given host:port, NULL if error.
     */
    <C> Connector<C> createConnector(SelectableChannel fd, C context);
}
