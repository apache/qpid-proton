/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.apache.qpid.proton.hawtdispatch.impl;

import org.fusesource.hawtbuf.Buffer;
import org.fusesource.hawtdispatch.transport.AbstractProtocolCodec;

import java.io.IOException;

/**
 * A HawtDispatch protocol codec that encodes/decodes AMQP 1.0 frames.
 *
 * @author <a href="http://hiramchirino.com">Hiram Chirino</a>
 */
public class AmqpProtocolCodec extends AbstractProtocolCodec {

    int maxFrameSize = 4*1024*1024;

    @Override
    protected void encode(Object object) throws IOException {
        nextWriteBuffer.write((Buffer) object);
    }

    @Override
    protected Action initialDecodeAction() {
        return new Action() {
            public Object apply() throws IOException {
                Buffer magic = readBytes(8);
                if (magic != null) {
                    nextDecodeAction = readFrameSize;
                    return new AmqpHeader(magic);
                } else {
                    return null;
                }
            }
        };
    }

    private final Action readFrameSize = new Action() {
        public Object apply() throws IOException {
            Buffer sizeBytes = peekBytes(4);
            if (sizeBytes != null) {
                int size = sizeBytes.bigEndianEditor().readInt();
                if (size < 8) {
                    throw new IOException(String.format("specified frame size %d is smaller than minimum frame size", size));
                }
                if( size > maxFrameSize ) {
                    throw new IOException(String.format("specified frame size %d is larger than maximum frame size", size));
                }

                // TODO: check frame min and max size..
                nextDecodeAction = readFrame(size);
                return nextDecodeAction.apply();
            } else {
                return null;
            }
        }
    };


    private final Action readFrame(final int size) {
        return new Action() {
            public Object apply() throws IOException {
                Buffer frameData = readBytes(size);
                if (frameData != null) {
                    nextDecodeAction = readFrameSize;
                    return frameData;
                } else {
                    return null;
                }
            }
        };
    }

    public int getReadBytesPendingDecode() {
        return readBuffer.position() - readStart;
    }

    public void skipProtocolHeader() {
        nextDecodeAction = readFrameSize;
    }

    public void readProtocolHeader() {
        nextDecodeAction = initialDecodeAction();
    }

    public int getMaxFrameSize() {
        return maxFrameSize;
    }

    public void setMaxFrameSize(int maxFrameSize) {
        this.maxFrameSize = maxFrameSize;
    }
}
