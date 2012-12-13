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

import java.util.EnumSet;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.EndpointError;
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.Receiver;
import org.apache.qpid.proton.engine.Sender;
import org.apache.qpid.proton.engine.Session;
import org.apache.qpid.proton.jni.Proton;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_session_t;

public class JNISession implements Session
{
    private SWIGTYPE_p_pn_session_t _impl;
    private Object _context;
    private JNIConnection _connection;

    JNISession(SWIGTYPE_p_pn_session_t session_t)
    {
        _impl = session_t;
        Proton.pn_session_set_context(_impl, this);
        _connection = JNIConnection.getConnection(Proton.pn_session_connection(_impl));
    }

    @Override
    public Sender sender(String name)
    {
        return new JNISender(Proton.pn_sender(_impl, name));
    }

    @Override
    public Receiver receiver(String name)
    {
        //TODO
        return new JNIReceiver(Proton.pn_receiver(_impl, name));
    }

    @Override
    public Session next(EnumSet<EndpointState> local, EnumSet<EndpointState> remote)
    {
        SWIGTYPE_p_pn_session_t session = Proton.pn_session_next(_impl, StateConverter.getStateMask(local, remote));
        if(session != null)
        {
            return (Session) Proton.pn_session_get_context(session);
        }
        return null;
    }

    @Override
    public Connection getConnection()
    {
        return _connection;
    }

    @Override
    public EndpointState getLocalState()
    {
        return StateConverter.getLocalState(Proton.pn_session_state(_impl));
    }

    @Override
    public EndpointState getRemoteState()
    {
        return StateConverter.getRemoteState(Proton.pn_session_state(_impl));
    }

    @Override
    public EndpointError getLocalError()
    {
        //TODO
        return null;
    }

    @Override
    public EndpointError getRemoteError()
    {
        //TODO
        return null;
    }

    @Override
    public void free()
    {
        if(_impl != null)
        {
            Proton.pn_session_set_context(_impl, null);
            Proton.pn_session_free(_impl);
            _impl = null;
        }

    }

    @Override
    public void open()
    {
        Proton.pn_session_open(_impl);
    }

    @Override
    public void close()
    {
        Proton.pn_session_close(_impl);
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

    static JNISession getSession(SWIGTYPE_p_pn_session_t session_t)
    {
        if(session_t != null)
        {
            JNISession sessionObj = (JNISession) Proton.pn_session_get_context(session_t);
            if(sessionObj == null)
            {
                sessionObj = new JNISession(session_t);
            }
            return sessionObj;
        }
        return null;
    }
}
