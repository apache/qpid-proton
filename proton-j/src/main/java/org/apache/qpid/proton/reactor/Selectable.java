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

package org.apache.qpid.proton.reactor;

import java.nio.channels.SelectableChannel;

import org.apache.qpid.proton.engine.Collector;
import org.apache.qpid.proton.engine.Handler;
import org.apache.qpid.proton.engine.Transport;

public interface Selectable extends ReactorChild {

    public interface Callback {
        void run(Selectable selectable);
    }

    public boolean isReading();

    boolean isWriting();

    long getDeadline() ;

    void setReading(boolean reading) ;

    void setWriting(boolean writing);

    void setDeadline(long deadline) ;

    public void onReadable(Callback runnable) ;

    public void onWritable(Callback runnable);

    public void onExpired(Callback runnable);

    public void onError(Callback runnable);

    public void onRelease(Callback runnable);

    public void onFinalize(Callback runnable);

    void readable() ;

    void writeable() ;

    void expired() ;

    void error();

    void release() ;

    void _finalize() ;

    // These are equivalent to the C code's set/get file descritor functions.
    void setChannel(SelectableChannel channel) ;

    public SelectableChannel getChannel() ;

    void setAttachment(Object attachment) ;

    Object getAttachment() ;

    boolean isRegistered() ;

    void setRegistered(boolean registered) ;

    void setCollector(final Collector collector) ;

    public Reactor getReactor() ;
    public void terminate() ;

    public enum RecordKeyType {
        PNI_TERMINATED
    }

    public enum RecordValueType {
        PN_VOID
    }

    public boolean hasRecord(RecordKeyType type);

    public void setRecord(RecordKeyType key, RecordValueType value) ;
    public boolean isTerminal();


    public Transport getTransport() ;

    public void setTransport(Transport transport) ;

    public void setReactor(Reactor reactor) ;

    public void add(Handler handler);

    public Handler getHandler() ;
}
