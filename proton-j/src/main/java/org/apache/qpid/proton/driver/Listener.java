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

/**
 * Server API.
 *
 * @param <C> application supplied context
 */
public interface Listener<C>
{
    /**
     * Accept a connection that is pending on the listener.
     *
     * @return a new connector for the remote, or NULL on error.
     */
    Connector<C> accept();

    /**
     * Access the application context that is associated with the listener.
     *
     * @return the application context that was passed when creating this
     *         listener. See {@link Driver#createListener(String, int, Object)
     *         createListener(String, int, Object)} and
     *         {@link Driver#createConnector(java.nio.channels.SelectableChannel, Object)
     *         createConnector(java.nio.channels.SelectableChannel, Object)}
     */
    C getContext();

    /**
     * Set the application context that is associated with this listener.
     *
     */
    void setContext(C ctx);

    /**
     * Close the socket used by the listener.
     *
     */
    void close() throws java.io.IOException;
}
