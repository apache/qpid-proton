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

import static java.util.Arrays.copyOfRange;
import static org.apache.qpid.proton.engine.impl.TransportTestHelper.assertByteArrayContentEquals;
import static org.apache.qpid.proton.engine.impl.TransportTestHelper.assertByteBufferContentEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.nio.ByteBuffer;

import org.junit.Test;

public class TransportOutputAdaptorTest
{
    private final CannedTransportOutputWriter _transportOutputWriter = new CannedTransportOutputWriter();
    private final TransportOutput _transportOutput = new TransportOutputAdaptor(_transportOutputWriter, 1024);

    @Test
    public void testThatOutputBufferIsReadOnly()
    {
        assertTrue(_transportOutput.head().isReadOnly());
    }

    @Test
    public void testGetOutputBuffer_containsCorrectBytes()
    {
        byte[] testBytes = "testbytes".getBytes();
        _transportOutputWriter.setNextCannedOutput(testBytes);

        assertEquals(testBytes.length, _transportOutput.pending());
        final ByteBuffer outputBuffer = _transportOutput.head();
        assertEquals(testBytes.length, outputBuffer.remaining());

        byte[] outputBytes = new byte[testBytes.length];
        outputBuffer.get(outputBytes);
        assertByteArrayContentEquals(testBytes, outputBytes);

        _transportOutput.pop(outputBuffer.position());

        final ByteBuffer outputBuffer2 = _transportOutput.head();
        assertEquals(0, outputBuffer2.remaining());
    }

    @Test
    public void testClientConsumesOutputInMultipleChunks()
    {
        byte[] testBytes = "testbytes".getBytes();
        _transportOutputWriter.setNextCannedOutput(testBytes);

        // sip the first two bytes into a small byte array

        int chunk1Size = 2;
        int chunk2Size = testBytes.length - chunk1Size;

        {
            final ByteBuffer outputBuffer1 = _transportOutput.head();
            byte[] byteArray1 = new byte[chunk1Size];

            outputBuffer1.get(byteArray1);
            assertEquals(chunk2Size, outputBuffer1.remaining());
            assertByteArrayContentEquals(copyOfRange(testBytes, 0, chunk1Size), byteArray1);

            _transportOutput.pop(outputBuffer1.position());
        }

        {
            final ByteBuffer outputBuffer2 = _transportOutput.head();
            int chunk2Offset = chunk1Size;
            assertByteBufferContentEquals(copyOfRange(testBytes, chunk2Offset, testBytes.length), outputBuffer2);
        }
    }

    @Test
    public void testClientConsumesOutputInMultipleChunksWithAdditionalTransportWriterOutput()
    {
        byte[] initialBytes = "abcd".getBytes();
        _transportOutputWriter.setNextCannedOutput(initialBytes);

        // sip the first two bytes into a small byte array
        int chunk1Size = 2;
        int initialRemaining = initialBytes.length - chunk1Size;

        {
            final ByteBuffer outputBuffer1 = _transportOutput.head();
            byte[] byteArray1 = new byte[chunk1Size];

            outputBuffer1.get(byteArray1);
            assertEquals(initialRemaining, outputBuffer1.remaining());
            assertByteArrayContentEquals(copyOfRange(initialBytes, 0, chunk1Size), byteArray1);

            _transportOutput.pop(outputBuffer1.position());
        }

        byte[] additionalBytes = "wxyz".getBytes();
        _transportOutputWriter.setNextCannedOutput(additionalBytes);

        {
            final ByteBuffer outputBuffer2 = _transportOutput.head();

            byte[] expectedBytes = "cdwxyz".getBytes();
            assertByteBufferContentEquals(expectedBytes, outputBuffer2);
        }
    }

    private static final class CannedTransportOutputWriter implements TransportOutputWriter
    {

        byte[] _cannedOutput = new byte[0];

        @Override
        public boolean writeInto(ByteBuffer outputBuffer)
        {
            int bytesWritten = ByteBufferUtils.pourArrayToBuffer(_cannedOutput, 0, _cannedOutput.length, outputBuffer);
            if(bytesWritten < _cannedOutput.length)
            {
                fail("Unable to write all " + _cannedOutput.length + " bytes of my canned output to the provided output buffer: " + outputBuffer);
            }
            _cannedOutput = new byte[0];
            return false;
        }

        void setNextCannedOutput(byte[] cannedOutput)
        {
            _cannedOutput = cannedOutput;
        }

        public void closed()
        {
            // do nothing
        }
    }
}
