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

import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Sasl;

/**
 * Client API
 *
 * @param <C> application supplied context
 */
public interface Connector<C>
{
    /** Service the given connector.
     *
     * Handle any inbound data, outbound data, or timing events pending on
     * the connector.
     *
     */
    void process();

    /** Access the listener which opened this connector.
     *
     * @return the listener which created this connector, or NULL if the
     *         connector has no listener (e.g. an outbound client
     *         connection).
     */
    @SuppressWarnings("rawtypes")
    Listener listener();

    /** Access the Authentication and Security context of the connector.
     *
     * @return the Authentication and Security context for the connector,
     *         or NULL if none.
     */
    Sasl sasl();

    /** Access the AMQP Connection associated with the connector.
     *
     * @return the connection context for the connector, or NULL if none.
     */
    Connection getConnection();

    /** Assign the AMQP Connection associated with the connector.
     *
     * @param connection the connection to associate with the connector.
     */
    void setConnection(Connection connection);

    /**
     * Access the application context that is associated with the connector.
     *
     * @return the application context that was passed when creating this
     *         connector. See
     *         {@link Driver#createConnector(String, int, Object)
     *         createConnector(String, int, Object)} and
     *         {@link Driver#createConnector(java.nio.channels.SelectableChannel, Object)
     *         createConnector(java.nio.channels.SelectableChannel, Object)}.
     */
    C getContext();

    /** Assign a new application context to the connector.
     *
     * @param context new application context to associate with the connector
     */
    void setContext(C context);

    /** Close the socket used by the connector.
     *
     */
    void close();

    /** Determine if the connector is closed.
     *
     * @return True if closed, otherwise false
     */
    boolean isClosed();

    /** Destructor for the given connector.
     *
     * Assumes the connector's socket has been closed prior to call.
     *
     */
    void destroy();
}
