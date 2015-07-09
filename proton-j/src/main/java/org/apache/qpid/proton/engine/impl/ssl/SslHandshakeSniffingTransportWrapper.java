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
package org.apache.qpid.proton.engine.impl.ssl;

import org.apache.qpid.proton.engine.impl.HandshakeSniffingTransportWrapper;
import org.apache.qpid.proton.engine.impl.TransportWrapper;


/**
 * SslHandshakeSniffingTransportWrapper
 *
 */

public class SslHandshakeSniffingTransportWrapper extends HandshakeSniffingTransportWrapper<SslTransportWrapper, TransportWrapper>
    implements SslTransportWrapper
{

    SslHandshakeSniffingTransportWrapper(SslTransportWrapper ssl, TransportWrapper plain) {
        super(ssl, plain);
    }

    @Override
    public String getCipherName()
    {
        if(isSecureWrapperSelected())
        {
            return _wrapper1.getCipherName();
        }
        else
        {
            return null;
        }
    }


    @Override
    public String getProtocolName()
    {
        if (isSecureWrapperSelected())
        {
            return _wrapper1.getProtocolName();
        }
        else
        {
            return null;
        }
    }

    private boolean isSecureWrapperSelected()
    {
        return _selectedTransportWrapper == _wrapper1;
    }

    protected int bufferSize() {
        // minimum length for determination
        return 5;
    }

    protected void makeDetermination(byte[] bytesInput)
    {
        boolean isSecure = checkForSslHandshake(bytesInput);
        if (isSecure)
        {
            _selectedTransportWrapper = _wrapper1;
        }
        else
        {
            _selectedTransportWrapper = _wrapper2;
        }
    }

    // TODO perhaps the sniffer should save up the bytes from each
    // input call until it has sufficient bytes to make the determination
    // and only then pass them to the secure or plain wrapped transport?
    private boolean checkForSslHandshake(byte[] buf)
    {
        if (buf.length >= bufferSize())
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
