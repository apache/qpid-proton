/*
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
 */
package org.apache.qpid.proton.engine.impl;

import java.nio.ByteBuffer;


/**
 * Thinly wraps a {@link ByteBuffer} to make it easy to be used for the following sequence of operations:
 * <ol>
 * <li>Append some more bytes (of any size) to me using {@link #appendAndClear(byte[], int, int)}</li>
 * <li>Read some of my bytes</li>
 * <li>Set my contents using {@link #set(ByteBuffer, int)}</li>
 * </ol>
 *
 * @see SimpleSslTransportWrapper
 */
public class BytePipeline
{
    private static final ByteBuffer EMPTY = ByteBuffer.wrap(new byte[0]);

    private ByteBuffer _bytes = EMPTY;

    /**
     * Append the new bytes to my existing not-yet-consumed ones.
     *
     * newBytes is copied, to guard against the caller subsequently modifying its contents.
     *
     * @return a read-only ByteBuffer containing the existiduplicateng contents plus the portion of newBytes implied by offset and size.
     * The returned object is intentionally not a copy; this allows clients to "consume" bytes by calling the the get(..) methods on it.
     */
    public ByteBuffer appendAndClear(byte[] newBytes, int offset, int numberOfNewBytesToAppend)
    {
        byte[] oldBytes = _bytes.array();
        int numberOfOldBytes = _bytes.remaining();
        int sizeOfResult = numberOfOldBytes + numberOfNewBytesToAppend;
        byte[] resultBytes = new byte[sizeOfResult];

        System.arraycopy(oldBytes, _bytes.position(), resultBytes, 0, numberOfOldBytes);
        System.arraycopy(newBytes, offset, resultBytes, numberOfOldBytes, numberOfNewBytesToAppend);

        ByteBuffer retVal = ByteBuffer.wrap(resultBytes);
        _bytes = EMPTY;

        return retVal;
    }

    /**
     * Stores the portion of newBytes implied by the offset. Does not take a copy of the provided buffer's content
     * because we assume no further changes will be made to it.
     */
    public void set(ByteBuffer newBytes, int offset)
    {
        _bytes = newBytes.duplicate();
        _bytes.clear();
        _bytes.position(offset);
    }

    /**
     * Get the number of bytes not yet consumed
     */
    public int getSize()
    {
        return _bytes.remaining();
    }

}
