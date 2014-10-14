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
package org.apache.qpid.proton.examples;

import java.nio.ByteBuffer;
import java.util.Arrays;


/**
 * Message
 *
 */

public class Message
{
    private final byte[] bytes;

    /**
     * These bytes are expected to be AMQP encoded.
     */
    public Message(byte[] bytes) {
        this.bytes = bytes;
    }

    private static final byte[] PREFIX = {(byte)0x00, (byte)0x53, (byte)0x77, (byte)0xb1};

    private static byte[] encodeString(String string) {
        byte[] utf8 = string.getBytes();
        byte[] result = new byte[PREFIX.length + 4 + utf8.length];
        ByteBuffer bbuf = ByteBuffer.wrap(result);
        bbuf.put(PREFIX);
        bbuf.putInt(utf8.length);
        bbuf.put(utf8);
        return result;
    }

    public Message(String string) {
        // XXX: special case string encoding for now
        this(encodeString(string));
    }

    public byte[] getBytes() {
        return bytes;
    }

    public String toString() {
        StringBuilder bld = new StringBuilder();
        bld.append("Message(");
        for (byte b : bytes) {
            if (b >= 32 && b < 127) {
                bld.append((char) b);
            } else {
                bld.append("\\x");
                String hex = Integer.toHexString(0xFF & b);
                if (hex.length() < 2) {
                    bld.append("0");
                }
                bld.append(hex);
            }
        }
        bld.append(')');
        return bld.toString();
    }

}
