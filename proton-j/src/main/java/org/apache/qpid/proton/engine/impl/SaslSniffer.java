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


/**
 * SaslSniffer
 *
 */

class SaslSniffer extends HandshakeSniffingTransportWrapper<TransportWrapper, TransportWrapper>
{

    SaslSniffer(TransportWrapper sasl, TransportWrapper other) {
        super(sasl, other);
    }

    protected int bufferSize() { return AmqpHeader.SASL_HEADER.length; }

    protected void makeDetermination(byte[] bytes) {
        if (bytes.length < bufferSize()) {
            throw new IllegalArgumentException("insufficient bytes");
        }

        for (int i = 0; i < AmqpHeader.SASL_HEADER.length; i++) {
            if (bytes[i] != AmqpHeader.SASL_HEADER[i]) {
                _selectedTransportWrapper = _wrapper2;
                return;
            }
        }

        _selectedTransportWrapper = _wrapper1;
    }

}
