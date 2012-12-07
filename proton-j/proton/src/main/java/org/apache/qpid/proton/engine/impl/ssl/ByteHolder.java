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
package org.apache.qpid.proton.engine.impl.ssl;

import java.nio.ByteBuffer;

import org.apache.qpid.proton.engine.impl.TransportInput;
import org.apache.qpid.proton.engine.impl.TransportOutput;

/**
 * I thinly wrap a {@link ByteBuffer} to facilitate its usage e.g. in {@link SimpleSslTransportWrapper}.
 * At a given moment I am either in a state that is ready to be written to, or read from.
 * To switch between these states, call {@link #prepareToRead()} or {@link #prepareToWrite()}, each of which returns the {@link ByteBuffer}
 * ready to be used.
 */
public class ByteHolder
{
    private final ByteBuffer _bytes;

    /** Creates me, initially in a writeable state */
    public ByteHolder(int capacity)
    {
        _bytes = ByteBuffer.allocate(capacity);
    }

    public boolean hasSpace()
    {
        return _bytes.remaining() > 0;
    }

    /**
     * @return the total number of bytes I now contain
     */
    public int writeOutputFrom(TransportOutput transportOutput)
    {
        byte[] byteArray = _bytes.array();
        int offset = _bytes.position();
        int numberOfBytesToGet = _bytes.remaining();

        int numberOfBytesGot = transportOutput.output(byteArray, offset, numberOfBytesToGet);

        int newPosition = offset + numberOfBytesGot;
        _bytes.position(newPosition);
        return newPosition;
    }

    /**
     * @return a ByteBuffer wrapping the stored bytes, intended to be read from
     */
    public ByteBuffer prepareToRead()
    {
        _bytes.flip();
        return _bytes;
    }

    /**
     * Read my bytes into the supplied destination
     * @return the number of byte actually read (might be less than numberOfBytesRequested if I contain fewer bytes than that)
     */
    public int readInto(byte[] destination, int offset, int numberOfBytesRequested)
    {
        int numberOfBytesToRead = Math.min(numberOfBytesRequested, _bytes.remaining());
        _bytes.get(destination, offset, numberOfBytesToRead);
        return numberOfBytesToRead;
    }

    /**
     * @return whether all my bytes were read into the {@link TransportInput} (i.e. returns false if transportInput didn't accept all the bytes)
     */
    public boolean readInto(TransportInput transportInput)
    {
        if(_bytes.hasRemaining())
        {
            int offset = _bytes.position();
            int numberAccepted = transportInput.input(_bytes.array(), offset, _bytes.remaining());
            int newPosition = offset + numberAccepted;
            _bytes.position(newPosition);
            return !_bytes.hasRemaining();
        }
        else
        {
            // nothing to do
            return true;
        }
    }

    /**
     * prepare me to be written to again
     * @return a ByteBuffer, ready to be written to.
     */
    public ByteBuffer prepareToWrite()
    {
        _bytes.compact();
        return _bytes;
    }

    public int getCapacity()
    {
        return _bytes.capacity();
    }

}
