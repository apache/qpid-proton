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

import static org.apache.qpid.proton.engine.impl.ssl.ByteTestHelper.assertArrayUntouchedExcept;
import static org.apache.qpid.proton.engine.impl.ssl.ByteTestHelper.assertByteBufferContents;
import static org.apache.qpid.proton.engine.impl.ssl.ByteTestHelper.createFilledBuffer;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.nio.ByteBuffer;

import org.apache.qpid.proton.engine.impl.ssl.ByteHolder;
import org.junit.Test;

public class ByteHolderTest
{
    private ByteHolder _byteHolder = new ByteHolder(10);

    @Test
    public void testEmptyOutput()
    {
        _byteHolder.writeOutputFrom(new CannedTransportOutput(""));
        assertByteBufferContents(_byteHolder.prepareToRead(), "");
    }

    @Test
    public void testWriteOutputFromWithSmallOutputPartiallyFillsHolder()
    {
        String smallOutput = "1234";
        assertTrue(smallOutput.length() < _byteHolder.getCapacity());

        _byteHolder.writeOutputFrom(new CannedTransportOutput(smallOutput));

        assertTrue(_byteHolder.hasSpace());
        assertByteBufferContents(_byteHolder.prepareToRead(), smallOutput);
    }

    @Test
    public void testWriteOutputFromWithOverflowingOutputCompletelyFillsHolder()
    {
        String bigOutput = "1234567890xxxxx";
        assertTrue(bigOutput.length() > _byteHolder.getCapacity());

        _byteHolder.writeOutputFrom(new CannedTransportOutput(bigOutput));
        assertFalse(_byteHolder.hasSpace());
        assertByteBufferContents(_byteHolder.prepareToRead(), "1234567890");
    }

    @Test
    public void testWriteOutputFromWithOutputMatchingCapacityCompletelyFillsHolder()
    {
        String bigOutput = "1234567890";
        assertTrue("Neither too big nor too small - the Goldilocks case", bigOutput.length() == _byteHolder.getCapacity());

        _byteHolder.writeOutputFrom(new CannedTransportOutput(bigOutput));
        assertFalse(_byteHolder.hasSpace());
        assertByteBufferContents(_byteHolder.prepareToRead(), "1234567890");
    }

    /**
     * Tests the expected usage of {@link ByteHolder}, i.e. repeatedly doing a
     * write and then a (potentially partial) read.
     */
    @Test
    public void testWriteOutputFromMultipleTimes()
    {
        // write and then partially read
        {
            _byteHolder.writeOutputFrom(new CannedTransportOutput("12345"));
            ByteBuffer readableBytes = _byteHolder.prepareToRead();
            readableBytes.get(new byte[2]); // chomp the first two bytes ('1' and '2')
            _byteHolder.prepareToWrite();
        }

        // write enough to fill the holder and then completely read
        {
            _byteHolder.writeOutputFrom(new CannedTransportOutput("abcdefghijk"));
            ByteBuffer readableBytes = _byteHolder.prepareToRead();
            assertByteBufferContents(readableBytes, "345abcdefg", "Readable bytes should contain '345' left over from previouis partial read, plus as many of the new bytes 'abc...' as could fit");
            _byteHolder.prepareToWrite();
        }

        // write and then read nothing
        {
            _byteHolder.writeOutputFrom(new CannedTransportOutput("xy"));
            _byteHolder.prepareToRead();
            _byteHolder.prepareToWrite();
        }

        // write nothing and then read
        {
            _byteHolder.writeOutputFrom(new CannedTransportOutput(""));
            ByteBuffer readableBytes = _byteHolder.prepareToRead();
            assertByteBufferContents(readableBytes, "xy", "Readable bytes should still contain 'xy' written in previous step");
        }
    }

    @Test
    public void testReadIntoWhenContainingFewerBytesThanRequested()
    {
        testReadInto("12345", 6, 2, "12345");
    }

    @Test
    public void testReadIntoWhenContainingMoreBytesThanRequested()
    {
        testReadInto("12345", 2, 0, "12");
    }

    private void testReadInto(String contents, int numberOfBytesRequested, int offset, String expectedOutput)
    {
        _byteHolder.writeOutputFrom(new CannedTransportOutput(contents));
        _byteHolder.prepareToRead();

        byte[] destination = createFilledBuffer(10);

        int numberOfBytesRead = _byteHolder.readInto(destination, offset, numberOfBytesRequested);
        assertEquals("Unexpected number of bytes read", expectedOutput.length(), numberOfBytesRead);
        assertArrayUntouchedExcept(expectedOutput, destination, offset);
    }

    @Test
    public void testReadIntoTransportInputWhenAllBytesAreAccepted()
    {
        testReadIntoTransportInput("12345", "12345", null);
    }

    @Test
    public void testReadIntoTransportInputWhenNotAllAccepted()
    {
        testReadIntoTransportInput("12345", "12", 2);
    }

    private void testReadIntoTransportInput(String contents, String expectedAcceptedInput, Integer acceptLimit)
    {
        String myString = contents;
        _byteHolder.writeOutputFrom(new CannedTransportOutput(myString));
        _byteHolder.prepareToRead();

        RememberingTransportInput transportInput = new RememberingTransportInput();
        if(acceptLimit != null)
        {
            transportInput.setAcceptLimit(acceptLimit);
        }

        boolean acceptedAllBytes = _byteHolder.readInto(transportInput);

        boolean expectedAcceptedAllBytes = expectedAcceptedInput.length() == contents.length();

        assertEquals("Return value should show whether all bytes were accepted", expectedAcceptedAllBytes, acceptedAllBytes);
        assertEquals(expectedAcceptedInput, transportInput.getAcceptedInput());
    }
}
