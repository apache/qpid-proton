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
        int i = Proton.pn_transport_input(_impl, ByteBuffer.wrap(bytes, offset, size));
        if(i == Proton.PN_ERR)
        {
            SWIGTYPE_p_pn_error_t err = Proton.pn_transport_error(_impl);
            String errorText = Proton.pn_error_text(err);
            Proton.pn_error_clear(err);
            throw new TransportException(errorText);
        }
        //System.err.println("**RG**  input: " + i);
        return i;
    }

    @Override
    public int output(byte[] bytes, int offset, int size)
    {
        /*int i = Proton.pn_transport_output(_impl, ByteBuffer.wrap(bytes, offset, size));
        return i;*/
        return Proton.pn_transport_output(_impl, ByteBuffer.wrap(bytes, offset, size));
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
    public EndpointError getLocalError()
    {
        return null; //TODO
    }

    @Override
    public EndpointError getRemoteError()
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

    public static void main(String args[])
    {
        System.loadLibrary("protonjni");
        JNITransport t1 = new JNITransport();
        JNITransport t2 = new JNITransport();
        JNIConnection conn1 = new JNIConnection();
        JNIConnection conn2 = new JNIConnection();
        t1.bind(conn1);
        t2.bind(conn2);

        conn1.open();
        conn2.open();

        pump(t1, t2);



    }

    private static void pump(JNITransport t1, JNITransport t2)
    {
        int len = 0;
        int len2 = 0;
        byte[] buf = new byte[10240];
        do
        {
        len = t1.output(buf,0,10240);
        if(len>0) t2.input(buf,0,len);
        len2 = t2.output(buf,0,10240);
        if(len2>0) t1.input(buf,0,len2);
        }
        while(len != 0 || len2 != 0);
    }

}
