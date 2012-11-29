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
package org.apache.qpid.proton.engine.impl;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;

import org.apache.qpid.proton.engine.TransportWrapper;
import org.junit.Test;

public class SslHandshakeSniffingTransportWrapperTest
{
    private static final byte[] EXAMPLE_SSL_V3_HANDSHAKE_BYTES = new byte[] {0x16, 0x03, 0x02, 0x00, 0x31};
    private static final byte[] EXAMPLE_SSL_V2_HANDSHAKE_BYTES = new byte[] {0x00, 0x00, 0x01, 0x03, 0x00};

    private SslTransportWrapper _secureTransportWrapper = mock(SslTransportWrapper.class);
    private TransportWrapper _plainTransportWrapper = mock(TransportWrapper.class);
    private TransportWrapper _sniffingWrapper = new SslHandshakeSniffingTransportWrapper(_secureTransportWrapper, _plainTransportWrapper);

    @Test
    public void testWithNonSsl()
    {
        byte[] sourceBuffer = "HELLO WORLD".getBytes();
        _sniffingWrapper.input(sourceBuffer, 0, sourceBuffer.length);

        verify(_plainTransportWrapper).input(sourceBuffer, 0, sourceBuffer.length);

        byte[] destinationBuffer = new byte[100];
        _sniffingWrapper.output(destinationBuffer, 0, destinationBuffer.length);

        verify(_plainTransportWrapper).output(destinationBuffer, 0, destinationBuffer.length);

        verifyZeroInteractions(_secureTransportWrapper);
    }

    @Test
    public void testWithSSLv2()
    {
        byte[] sourceBuffer = EXAMPLE_SSL_V2_HANDSHAKE_BYTES;
        _sniffingWrapper.input(sourceBuffer, 0, sourceBuffer.length);

        verify(_secureTransportWrapper).input(sourceBuffer, 0, sourceBuffer.length);

        byte[] destinationBuffer = new byte[100];
        _sniffingWrapper.output(destinationBuffer, 0, destinationBuffer.length);

        verify(_secureTransportWrapper).output(destinationBuffer, 0, destinationBuffer.length);

        verifyZeroInteractions(_plainTransportWrapper);
    }

    @Test
    public void testWithSSLv3TLS()
    {
        byte[] sourceBuffer = EXAMPLE_SSL_V3_HANDSHAKE_BYTES;
        _sniffingWrapper.input(sourceBuffer, 0, sourceBuffer.length);

        verify(_secureTransportWrapper).input(sourceBuffer, 0, sourceBuffer.length);

        byte[] destinationBuffer = new byte[100];
        _sniffingWrapper.output(destinationBuffer, 0, destinationBuffer.length);

        verify(_secureTransportWrapper).output(destinationBuffer, 0, destinationBuffer.length);

        verifyZeroInteractions(_plainTransportWrapper);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testTooFewBytesToMakeDetermination()
    {
        byte[] sourceBuffer = new byte[] {0x00};

        try
        {
            _sniffingWrapper.input(sourceBuffer, 0, sourceBuffer.length);
        }
        finally
        {
            verifyZeroInteractions(_secureTransportWrapper);
            verifyZeroInteractions(_plainTransportWrapper);
        }
    }

}
