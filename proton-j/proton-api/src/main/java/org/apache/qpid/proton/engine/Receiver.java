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


/**
 * Receiver
 *
 */
public interface Receiver extends Link
{

    /**
     * Adds the specified number of credits.
     *
     * The number of link credits initialises to zero.  It is the application's responsibility to call
     * this method to allow the receiver to receive {@code credits} more deliveries.
     */
    public void flow(int credits);

    /**
     * Receive message data for the current delivery.
     *
     * If the caller takes all the bytes the Receiver currently has for this delivery then it is removed from
     * the Connection's work list.
     *
     * Before considering a delivery to be complete, the caller should examine {@link Delivery#isPartial()}.  If
     * the delivery is partial, the caller should call {@link #recv(byte[], int, int)} again to receive
     * the additional bytes once the Delivery appears again on the Connection work-list.
     *
     * TODO might the flags other than IO_WORK in DeliveryImpl also prevent the work list being pruned?
     * e.g. what if a slow JMS consumer receives a disposition frame containing state=RELEASED? This is not IO_WORK.
     *
     * @param bytes the destination array where the message data is written
     * @param offset index in the array to start writing data at
     * @param size the maximum number of bytes to write
     *
     * @return number of bytes written. -1 if there are no more bytes for the current delivery.
     *
     * @see #current()
     */
    public int recv(byte[] bytes, int offset, int size);

    public void drain(int credit);

    /**
     * {@inheritDoc}
     *
     * TODO document what this method conceptually does and when you should use it.
     */
    @Override
    public boolean advance();

    public boolean draining();

    public void setDrain(boolean drain);

}
