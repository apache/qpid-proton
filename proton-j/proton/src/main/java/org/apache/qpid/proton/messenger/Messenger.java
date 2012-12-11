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
package org.apache.qpid.proton.messenger;

import java.io.IOException;
import java.util.concurrent.TimeoutException;
import org.apache.qpid.proton.message.Message;

public interface Messenger
{
    static final int CUMULATIVE = 0x01;

    void put(Message message) throws MessengerException;
    void send() throws TimeoutException;

    void subscribe(String source) throws MessengerException;
    void recv(int count) throws TimeoutException;
    Message get();

    void start() throws IOException;
    void stop();

    void setTimeout(long timeInMillis);
    long getTimeout();

    int outgoing();
    int incoming();

    AcceptMode getAcceptMode();
    void setAcceptMode(AcceptMode mode);

    int getIncomingWindow();
    void setIncomingWindow(int window);

    int getOutgoingWindow();
    void setOutgoingWindow(int window);

    Tracker incomingTracker();
    Tracker outgoingTracker();

    void reject(Tracker tracker, int flags);
    void accept(Tracker tracker, int flags);
    void settle(Tracker tracker, int flags);

    Status getStatus(Tracker tracker);
}
