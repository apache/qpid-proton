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
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.channels.Pipe;
import java.nio.channels.Pipe.SinkChannel;
import java.nio.channels.Pipe.SourceChannel;
import java.util.concurrent.atomic.AtomicInteger;

public class EchoInputStreamWrapper extends Thread {

    private final InputStream in;
    private final SinkChannel out;
    private final byte[] bufferBytes = new byte[1024];
    private final ByteBuffer buffer = ByteBuffer.wrap(bufferBytes);
    private final AtomicInteger idCounter = new AtomicInteger();

    private EchoInputStreamWrapper(InputStream in, SinkChannel out) {
        this.in = in;
        this.out = out;
        setName(getClass().getName() + "-" + idCounter.incrementAndGet());
        setDaemon(true);
    }

    @Override
    public void run() {
        try {
            while(true) {
                int amount = in.read(bufferBytes);
                if (amount < 0) break;
                buffer.position(0);
                buffer.limit(amount);
                out.write(buffer);
            }
        } catch(IOException ioException) {
            ioException.printStackTrace();
        } finally {
            try {
                out.close();
            } catch(IOException ioException) {
                ioException.printStackTrace();
            }
        }
    }

    public static SourceChannel wrap(InputStream in) throws IOException {
        Pipe pipe = Pipe.open();
        new EchoInputStreamWrapper(in, pipe.sink()).start();
        SourceChannel result = pipe.source();
        result.configureBlocking(false);
        return result;
    }

}
