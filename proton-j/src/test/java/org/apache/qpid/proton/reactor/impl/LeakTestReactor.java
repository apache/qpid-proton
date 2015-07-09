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

package org.apache.qpid.proton.reactor.impl;

import java.io.IOException;
import java.nio.channels.Pipe;
import java.nio.channels.Pipe.SinkChannel;
import java.nio.channels.Pipe.SourceChannel;
import java.nio.channels.Selector;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import java.util.HashMap;
import java.util.Map.Entry;

import junit.framework.AssertionFailedError;

// Extends the Reactor to substitute a unit-test implementation of the
// IO class.  This detects, and reports, situations where the reactor code
// fails to close one of the Java I/O related resources that it has created.
public class LeakTestReactor extends ReactorImpl {

    private static class TestIO implements IO {

        private final HashMap<Object, Exception> resources = new HashMap<Object, Exception>();

        @Override
        public Pipe pipe() throws IOException {
            Pipe pipe = Pipe.open();
            resources.put(pipe.source(), new Exception());
            resources.put(pipe.sink(), new Exception());
            return pipe;
        }

        @Override
        public Selector selector() throws IOException {
            Selector selector = Selector.open();
            resources.put(selector, new Exception());
            return selector;

        }

        @Override
        public ServerSocketChannel serverSocketChannel() throws IOException {
            ServerSocketChannel serverSocketChannel = ServerSocketChannel.open();
            resources.put(serverSocketChannel, new Exception());
            return serverSocketChannel;
        }

        @Override
        public SocketChannel socketChannel() throws IOException {
            SocketChannel socketChannel = SocketChannel.open();
            resources.put(socketChannel, new Exception());
            return socketChannel;
        }

        private boolean isOpen(Object resource) {
            if (resource instanceof SourceChannel) {
                return ((SourceChannel)resource).isOpen();
            } else if (resource instanceof SinkChannel) {
                return ((SinkChannel)resource).isOpen();
            } else if (resource instanceof Selector) {
                return ((Selector)resource).isOpen();
            } else if (resource instanceof ServerSocketChannel) {
                return ((ServerSocketChannel)resource).isOpen();
            } else if (resource instanceof SocketChannel) {
                return ((SocketChannel)resource).isOpen();
            } else {
                throw new AssertionFailedError("Don't know how to check if this type is open: " + resource.getClass());
            }
        }

        protected void assertNoLeaks() throws AssertionFailedError {
            boolean fail = false;
            for (Entry<Object, Exception> entry : resources.entrySet()) {
                if (isOpen(entry.getKey())) {
                    System.out.println("Leaked an instance of '" + entry.getKey() + "' from:");
                    entry.getValue().printStackTrace(System.out);
                    fail = true;
                }
            }
            if (fail) {
                throw new AssertionFailedError("Resources leaked");
            }
            resources.clear();
        }
    }

    private final TestIO testIO;

    public LeakTestReactor() throws IOException {
        super(new TestIO());
        testIO = (TestIO)getIO();
    }

    public void assertNoLeaks() throws AssertionFailedError {
        testIO.assertNoLeaks();
    }

}
