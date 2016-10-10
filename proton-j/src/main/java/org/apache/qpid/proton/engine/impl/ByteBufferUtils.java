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
 *
 */
package org.apache.qpid.proton.engine.impl;

import java.nio.ByteBuffer;

import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.engine.TransportException;

public class ByteBufferUtils
{
    /**
     * @return number of bytes poured
     */
    public static int pour(ByteBuffer source, ByteBuffer destination)
    {
        int numberOfBytesToPour = Math.min(source.remaining(), destination.remaining());
        ByteBuffer sourceSubBuffer = source.duplicate();
        sourceSubBuffer.limit(sourceSubBuffer.position() + numberOfBytesToPour);
        destination.put(sourceSubBuffer);
        source.position(source.position() + numberOfBytesToPour);
        return numberOfBytesToPour;
    }

    /**
     * Assumes {@code destination} is ready to be written.
     *
     * @return number of bytes poured which may be fewer than {@code sizeRequested} if
     * {@code destination} has insufficient remaining
     */
    public static int pourArrayToBuffer(byte[] source, int offset, int sizeRequested, ByteBuffer destination)
    {
        int numberToWrite = Math.min(destination.remaining(), sizeRequested);
        destination.put(source, offset, numberToWrite);
        return numberToWrite;
    }

    /**
     * Pours the contents of {@code source} into {@code destinationTransportInput}, calling
     * the TransportInput many times if necessary.  If the TransportInput returns a {@link org.apache.qpid.proton.engine.TransportResult}
     * other than ok, data may remain in source.
     */
    public static int pourAll(ByteBuffer source, TransportInput destinationTransportInput) throws TransportException
    {
        int capacity = destinationTransportInput.capacity();
        if (capacity == Transport.END_OF_STREAM)
        {
            if (source.hasRemaining()) {
                throw new IllegalStateException("Destination has reached end of stream: " +
                                                destinationTransportInput);
            } else {
                return Transport.END_OF_STREAM;
            }
        }

        int total = source.remaining();

        while(source.hasRemaining() && destinationTransportInput.capacity() > 0)
        {
            pour(source, destinationTransportInput.tail());
            destinationTransportInput.process();
        }

        return total - source.remaining();
    }

    /**
     * Assumes {@code source} is ready to be read.
     *
     * @return number of bytes poured which may be fewer than {@code sizeRequested} if
     * {@code source} has insufficient remaining
     */
    public static int pourBufferToArray(ByteBuffer source, byte[] destination, int offset, int sizeRequested)
    {
        int numberToRead = Math.min(source.remaining(), sizeRequested);
        source.get(destination, offset, numberToRead);
        return numberToRead;
    }

    public static ByteBuffer newWriteableBuffer(int capacity)
    {
        ByteBuffer newBuffer = ByteBuffer.allocate(capacity);
        return newBuffer;
    }

    public static ByteBuffer newReadableBuffer(int capacity)
    {
        ByteBuffer newBuffer = ByteBuffer.allocate(capacity);
        newBuffer.flip();
        return newBuffer;
    }



    /**
     * Format the ByteBuffer string representation into a programmer's understandable format
     */
    public static String formatGroup(String str, int groupSize, int lineBreak)
    {
       StringBuffer buffer = new StringBuffer();

       int line = 1;
       buffer.append("/*  1 */ \"");
       for (int i = 0; i < str.length(); i += groupSize)
       {
          buffer.append(str.substring(i, i + Math.min(str.length() - i, groupSize)));

          if ((i + groupSize) % lineBreak == 0)
          {
             buffer.append("\" +\n/* ");
             line++;
             if (line < 10)
             {
                buffer.append(" ");
             }
             buffer.append(Integer.toString(line) + " */ \"");
          }
          else if ((i + groupSize) % groupSize == 0 && str.length() - i > groupSize)
          {
             buffer.append("\" + \"");
          }
       }

       buffer.append("\";");

       return buffer.toString();

    }

    protected static final char[] hexArray = "0123456789ABCDEF".toCharArray();

    /**
     * Creates a String representation of the byte stream
     * @param bytes
     * @return
     */
    public static String bytesToHex(byte[] bytes)
    {
       char[] hexChars = new char[bytes.length * 2];
       for (int j = 0; j < bytes.length; j++)
       {
          int v = bytes[j] & 0xFF;
          hexChars[j * 2] = hexArray[v >>> 4];
          hexChars[j * 2 + 1] = hexArray[v & 0x0F];
       }
       return new String(hexChars);
    }


}
