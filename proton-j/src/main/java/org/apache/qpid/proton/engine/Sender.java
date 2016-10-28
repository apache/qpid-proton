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
package org.apache.qpid.proton.engine;

import org.apache.qpid.proton.codec.ReadableBuffer;

/**
 * Sender
 *
 */
public interface Sender extends Link
{

    /**
     * indicates pending deliveries
     *
     * @param credits the number of pending deliveries
     */
    //TODO is this absolute or cumulative?
    public void offer(int credits);

    /**
     * Sends some data for the current delivery.  The application may call this method multiple
     * times for the same delivery.
     *
     * @param bytes the byte array containing the data to be sent.
     * @param offset the offset into the given array to start reading.
     * @param length the number of bytes to read from the given byte array.
     *
     * @return the number of bytes accepted
     *
     * TODO Proton-j current copies all the bytes it has been given so the return value will always be
     * length.  Should this be changed? How does Proton-c behave?   What should the application do if
     * the number of bytes accepted is smaller than length.
     */
    public int send(byte[] bytes, int offset, int length);

    /**
     * Sends some data for the current delivery. The application may call this method multiple
     * times for the same delivery.
     *
     * @param buffer the buffer to read the data from.
     *
     * @return the number of bytes read from the provided buffer.
     */
    public int send(ReadableBuffer buffer);

    /**
     * Abort the current delivery.
     *
     * Note "pn_link_abort" is commented out in the .h
     */
    public void abort();

    /**
     * {@inheritDoc}
     *
     * Informs the sender that all the bytes of the current {@link Delivery} have been written.
     * The application must call this method in order for the delivery to be considered complete.
     *
     * @see #send(byte[], int, int)
     *
     * TODO fully state the rules regarding when you have to call this method, what happens if you don't call it
     * before creating another delivery etc.
     */
    @Override
    public boolean advance();

}
