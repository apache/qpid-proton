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

package org.apache.qpid.proton.engine.jni;

import static org.apache.qpid.proton.jni.ExceptionHelper.checkProtonCReturnValue;

import java.nio.ByteBuffer;
import java.nio.charset.Charset;

import org.apache.qpid.proton.ProtonCEquivalent;
import org.apache.qpid.proton.engine.Ssl;
import org.apache.qpid.proton.jni.Proton;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_ssl_t;

class JNISsl implements Ssl
{
    private final SWIGTYPE_p_pn_ssl_t _impl;

    JNISsl(SWIGTYPE_p_pn_ssl_t impl)
    {
        _impl = impl;
    }

    @Override
    @ProtonCEquivalent("pn_ssl_get_cipher_name")
    public String getCipherName()
    {
        byte[] data = new byte[1024];
        boolean b = Proton.pn_ssl_get_cipher_name(_impl, ByteBuffer.wrap(data));
        return b ? asString(data) : null;

    }

    private String asString(byte[] data)
    {
        int i = -1;
        while(data[++i] != 0);
        if(i == 0)
        {
            return null;
        }
        else
        {
            return new String(data,0,i,Charset.forName("US-ASCII"));
        }
    }

    @Override
    @ProtonCEquivalent("pn_ssl_get_protocol_name")
    public String getProtocolName()
    {
        byte[] data = new byte[1024];
        boolean b = Proton.pn_ssl_get_protocol_name(_impl, ByteBuffer.wrap(data));
        return b ? asString(data) : null;
    }

    @Override
    public void setPeerHostname(String hostname)
    {
        int retVal = Proton.pn_ssl_set_peer_hostname(_impl, hostname);
        checkProtonCReturnValue(retVal);
    }

    @Override
    public String getPeerHostname()
    {
        byte[] data = new byte[256]; // hostnames are a maximum of 255 characters long (see http://tools.ietf.org/html/rfc1034#section-3.1)
        int retVal = Proton.pn_ssl_get_peer_hostname(_impl, ByteBuffer.wrap(data));
        checkProtonCReturnValue(retVal);
        return asString(data);
    }
}
