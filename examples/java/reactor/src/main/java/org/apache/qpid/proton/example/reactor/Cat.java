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

import java.io.FileInputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.Pipe.SourceChannel;

import org.apache.qpid.proton.Proton;
import org.apache.qpid.proton.engine.BaseHandler;
import org.apache.qpid.proton.engine.Event;
import org.apache.qpid.proton.reactor.Reactor;
import org.apache.qpid.proton.reactor.Selectable;

public class Cat extends BaseHandler {

    private class EchoHandler extends BaseHandler {
        @Override
        public void onSelectableInit(Event event) {
            Selectable selectable = event.getSelectable();
            // We can configure a selectable with any SelectableChannel we want.
            selectable.setChannel(channel);
            // Ask to be notified when the channel is readable
            selectable.setReading(true);
            event.getReactor().update(selectable);
        }

        @Override
        public void onSelectableReadable(Event event) {
            Selectable selectable = event.getSelectable();

            // The onSelectableReadable event tells us that there is data
            // to be read, or the end of stream has been reached.
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

    private Cat(SourceChannel channel) {
        this.channel = channel;
    }

    @Override
    public void onReactorInit(Event event) {
        Reactor reactor = event.getReactor();
        Selectable selectable = reactor.selectable();
        setHandler(selectable, new EchoHandler());
        reactor.update(selectable);
    }

    public static void main(String[] args) throws IOException {
        if (args.length != 1) {
            System.err.println("Specify a file name as an argument.");
            System.exit(1);
        }
        FileInputStream inFile = new FileInputStream(args[0]);
        SourceChannel inChannel = EchoInputStreamWrapper.wrap(inFile);
        Reactor reactor = Proton.reactor(new Cat(inChannel));
        reactor.run();
    }
}
