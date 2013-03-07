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

import java.nio.ByteBuffer;

import org.apache.qpid.proton.ProtonCEquivalent;
import org.apache.qpid.proton.amqp.transport.ErrorCondition;
import org.apache.qpid.proton.engine.*;
import org.apache.qpid.proton.jni.Proton;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_connection_t;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_error_t;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_ssl_t;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_transport_t;

public class JNITransport implements Transport
{

    private SWIGTYPE_p_pn_transport_t _impl;
    private JNISasl _sasl;
    private Object _context;
    private JNISsl _ssl;


    public JNITransport()
    {
        _impl = Proton.pn_transport();
//        Proton.pn_transport_trace(_impl,Proton.PN_TRACE_FRM);
    }

    @Override
    @ProtonCEquivalent("pn_transport_bind")
    public void bind(Connection connection)
    {
        JNIConnection jniConn = (JNIConnection)connection;
        SWIGTYPE_p_pn_connection_t connImpl = jniConn.getImpl();
        Proton.pn_transport_bind(_impl, connImpl);
    }

    @Override
    @ProtonCEquivalent("pn_transport_input")
    public int input(byte[] bytes, int offset, int size)
    {
        int bytesConsumed = Proton.pn_transport_input(_impl, ByteBuffer.wrap(bytes, offset, size));
        if(bytesConsumed == Proton.PN_ERR)
        {
            SWIGTYPE_p_pn_error_t err = Proton.pn_transport_error(_impl);
            String errorText = Proton.pn_error_text(err);
            Proton.pn_error_clear(err);
            throw new TransportException(errorText);
        }
        else if (bytesConsumed == Proton.PN_EOS)
        {
            // TODO: PROTON-264 Proton-c returns PN_EOS after close has been consumed rather than
            // reporting the number of bytes consumed. This will be resolved by PROTON-225.
            return size;
        }
        return bytesConsumed;
    }

    @Override
    public int output(byte[] bytes, int offset, int size)
    {
        int bytesProduced = Proton.pn_transport_output(_impl, ByteBuffer.wrap(bytes, offset, size));
        if (bytesProduced == Proton.PN_EOS)
        {
            // TODO: PROTON-264 Proton-c returns PN_EOS after close has been consumed rather than
            // returning 0.
            return 0;
        }

        return bytesProduced;
    }

    @Override
    @ProtonCEquivalent("pn_sasl")
    public Sasl sasl()
    {
        if(_sasl == null)
        {
            _sasl = new JNISasl( Proton.pn_sasl(_impl));
        }
        return _sasl;

    }

    @Override
    @ProtonCEquivalent("pn_ssl")
    public Ssl ssl(SslDomain sslDomain, SslPeerDetails sslPeerDetails)
    {
        if(_ssl == null)
        {
            // TODO move this code to SslPeerDetails or its factory
            final String sessionId;
            if (sslPeerDetails == null)
            {
                sessionId = null;
            }
            else
            {
                sessionId = sslPeerDetails.getHostname() + ":" + sslPeerDetails.getPort();
            }

            SWIGTYPE_p_pn_ssl_t pn_ssl = Proton.pn_ssl( _impl );
            _ssl = new JNISsl( pn_ssl);
            Proton.pn_ssl_init(pn_ssl, ((JNISslDomain)sslDomain).getImpl(), sessionId);
            // TODO is the returned int an error code??
        }
        return _ssl;
    }

    @Override
    public Ssl ssl(SslDomain sslDomain)
    {
        return ssl(sslDomain, null);
    }

    @Override
    public EndpointState getLocalState()
    {
        return null; //TODO
    }

    @Override
    public EndpointState getRemoteState()
    {
        return null; //TODO
    }

    @Override
    public ErrorCondition getCondition()
    {
        return null; //TODO
    }

    @Override
    public void setCondition(ErrorCondition condition)
    {
        // TODO
    }

    @Override
    public ErrorCondition getRemoteCondition()
    {
        return null; //TODO
    }

    @Override
    public void free()
    {
        Proton.pn_transport_free(_impl);
    }

    @Override
    public void open()
    {
    }

    @Override
    public void close()
    {
    }

    @Override
    public void setContext(Object o)
    {
        _context = o;
    }

    @Override
    public Object getContext()
    {
        return _context;
    }

    @Override
    protected void finalize() throws Throwable
    {
        free();
        super.finalize();
    }
}
