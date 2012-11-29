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

import org.apache.qpid.proton.engine.TransportWrapper;

public class SslHandshakeSniffingTransportWrapper implements SslTransportWrapper
{
    private final SslTransportWrapper _secureTransportWrapper;
    private final TransportWrapper _plainTransportWrapper;

    private boolean _determinationMade = false;
    private boolean _isSecure;

    public SslHandshakeSniffingTransportWrapper(
            SslTransportWrapper secureTransportWrapper,
            TransportWrapper plainTransportWrapper)
    {
        _secureTransportWrapper = secureTransportWrapper;
        _plainTransportWrapper = plainTransportWrapper;
    }

    @Override
    public int input(byte[] sourceBuffer, int offset, int size)
    {
        if (_determinationMade==false)
        {
            byte[] zeroBasedSrcBytes = new byte[size];
            System.arraycopy(sourceBuffer, offset, zeroBasedSrcBytes, 0, size);

            _isSecure = checkForSslHandshake(zeroBasedSrcBytes);
            _determinationMade = true;
        }

        if (_isSecure)
        {
            return _secureTransportWrapper.input(sourceBuffer, offset, size);
        }
        else
        {
            return _plainTransportWrapper.input(sourceBuffer, offset, size);
        }
    }

    @Override
    public int output(byte[] destinationBuffer, int offset, int size)
    {
        if (_determinationMade == false)
        {
            _isSecure = false;
            _determinationMade = true;
        }

        if (_isSecure)
        {
            return _secureTransportWrapper.output(destinationBuffer, offset, size);
        }
        else
        {
            return _plainTransportWrapper.output(destinationBuffer, offset, size);
        }
    }

    @Override
    public String getCipherName()
    {
        return _secureTransportWrapper.getCipherName();
    }

    @Override
    public String getProtocolName()
    {
        return _secureTransportWrapper.getProtocolName();
    }

    // TODO perhaps the sniffer should save up the bytes from each
    // input call until it has sufficient bytes to make the determination
    // and only then pass them to the secure or plain wrapped transport?
    private boolean checkForSslHandshake(byte[] buf)
    {
        if (buf.length >= 5)
        {
            /*
             * SSLv2 Client Hello format
             * http://www.mozilla.org/projects/security/pki/nss/ssl/draft02.html
             *
             * Bytes 0-1: RECORD-LENGTH Byte 2: MSG-CLIENT-HELLO (1) Byte 3:
             * CLIENT-VERSION-MSB Byte 4: CLIENT-VERSION-LSB
             *
             * Allowed versions: 2.0 - SSLv2 3.0 - SSLv3 3.1 - TLS 1.0 3.2 - TLS
             * 1.1 3.3 - TLS 1.2
             *
             * The version sent in the Client-Hello is the latest version
             * supported by the client. NSS may send version 3.x in an SSLv2
             * header for maximum compatibility.
             */
            boolean isSSL2Handshake = buf[2] == 1 && // MSG-CLIENT-HELLO
                    ((buf[3] == 3 && buf[4] <= 3) || // SSL 3.0 & TLS 1.0-1.2
                                                     // (v3.1-3.3)
                    (buf[3] == 2 && buf[4] == 0)); // SSL 2

            /*
             * SSLv3/TLS Client Hello format RFC 2246
             *
             * Byte 0: ContentType (handshake - 22) Bytes 1-2: ProtocolVersion
             * {major, minor}
             *
             * Allowed versions: 3.0 - SSLv3 3.1 - TLS 1.0 3.2 - TLS 1.1 3.3 -
             * TLS 1.2
             */
            boolean isSSL3Handshake = buf[0] == 22 && // handshake
                    (buf[1] == 3 && buf[2] <= 3); // SSL 3.0 & TLS 1.0-1.2
                                                  // (v3.1-3.3)

            return (isSSL2Handshake || isSSL3Handshake);
        }
        else
        {
            throw new IllegalArgumentException("Too few bytes (" + buf.length + ") to make SSL/plain  determination.");
        }
    }
}
