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

package org.apache.qpid.proton.example.reactor;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.Pipe.SourceChannel;

import org.apache.qpid.proton.Proton;
import org.apache.qpid.proton.engine.BaseHandler;
import org.apache.qpid.proton.engine.Event;
import org.apache.qpid.proton.reactor.Reactor;
import org.apache.qpid.proton.reactor.Selectable;

public class Echo extends BaseHandler {

    private class EchoHandler extends BaseHandler {
        @Override
        public void onSelectableInit(Event event) {
            Selectable selectable = event.getSelectable();
            Reactor reactor = event.getReactor();
            selectable.setReading(true);
            reactor.update(selectable);
        }

        @Override
        public void onSelectableReadable(Event event) {
            Selectable selectable = event.getSelectable();
            SourceChannel channel = (SourceChannel)selectable.getChannel();
            ByteBuffer buffer = ByteBuffer.allocate(1024);
            try {
                while(true) {
                    int amount = channel.read(buffer);
                    if (amount < 0) {
                        selectable.terminate();
                        selectable.getReactor().update(selectable);
                    }
                    if (amount <= 0) break;
                    System.out.write(buffer.array(), 0, buffer.position());
                    buffer.clear();
                }
            } catch(IOException ioException) {
                ioException.printStackTrace();
                selectable.terminate();
                selectable.getReactor().update(selectable);
            }
        }
    }

    private final SourceChannel channel;

    private Echo(SourceChannel channel) {
        this.channel = channel;
    }

    @Override
    public void onReactorInit(Event event) {
        System.out.println("Type whatever you want and then use Control-D to exit:");
        Reactor reactor = event.getReactor();
        Selectable selectable = reactor.selectable();
        selectable.setChannel(channel);
        setHandler(selectable, new EchoHandler());
        reactor.update(selectable);
    }

    public static void main(String[] args) throws IOException {
        SourceChannel inChannel = EchoInputStreamWrapper.wrap(System.in);
        Reactor reactor = Proton.reactor(new Echo(inChannel));
        reactor.run();
    }
}
