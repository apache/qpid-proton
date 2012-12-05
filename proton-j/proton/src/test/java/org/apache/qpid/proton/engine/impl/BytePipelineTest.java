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

import static org.apache.qpid.proton.engine.impl.ByteTestHelper.assertByteBufferContents;
import static org.junit.Assert.assertEquals;

import java.nio.ByteBuffer;

import org.apache.qpid.proton.engine.impl.BytePipeline;
import org.junit.Test;

public class BytePipelineTest
{
    private BytePipeline _bytePipeline = new BytePipeline();

    @Test
    public void testApppendToNewPipeline()
    {
        ByteBuffer bytes = _bytePipeline.appendAndClear("123".getBytes(), 0, 3);
        assertByteBufferContents(bytes, "123");
    }

    @Test
    public void testApppend()
    {
        _bytePipeline.appendAndClear("123".getBytes(), 0, 3);
        ByteBuffer bytes = _bytePipeline.appendAndClear("456".getBytes(), 0, 3);
        assertByteBufferContents(bytes, "456");
    }

    @Test
    public void testAppendWithOffsetAndSize()
    {
        ByteBuffer bytes = _bytePipeline.appendAndClear("xx123x".getBytes(), 2, 3);
        assertByteBufferContents(bytes, "123");
    }

    @Test
    public void testGetSize()
    {
        _bytePipeline.set(ByteBuffer.wrap("123".getBytes()), 0);
        assertEquals(3, _bytePipeline.getSize());

        _bytePipeline.set(ByteBuffer.wrap("456".getBytes()), 0);
        assertEquals(3, _bytePipeline.getSize());
    }

    @Test
    public void testSet()
    {
        ByteBuffer bytesToSet = ByteBuffer.wrap("123".getBytes());
        _bytePipeline.set(bytesToSet, 0);
        assertContents("123");
    }

    @Test
    public void testSetWithOffset()
    {
        ByteBuffer bytesToSet = ByteBuffer.wrap("123".getBytes());
        _bytePipeline.set(bytesToSet, 1);
        assertContents("23");
    }

    @Test
    public void testSetDisregardsWhetherProvidedBufferAlreadyRead()
    {
        ByteBuffer bytesToSet = ByteBuffer.wrap("123".getBytes());
        bytesToSet.get(); // reading from the byte buffer should not affect the behaviour of set(..)
        _bytePipeline.set(bytesToSet, 1);
        assertContents("23");
    }

    private void assertContents(String expectedContents)
    {
        ByteBuffer bytes = _bytePipeline.appendAndClear("".getBytes(), 0, 0);
        assertByteBufferContents(bytes, expectedContents);
    }
}
