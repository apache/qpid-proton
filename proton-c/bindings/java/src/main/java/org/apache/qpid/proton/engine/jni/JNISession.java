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

import org.apache.qpid.proton.ProtonCEquivalent;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.transport.ErrorCondition;
import org.apache.qpid.proton.codec.Data;
import org.apache.qpid.proton.codec.jni.JNIData;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.Receiver;
import org.apache.qpid.proton.engine.Sender;
import org.apache.qpid.proton.engine.Session;
import org.apache.qpid.proton.jni.Proton;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_condition_t;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_session_t;

public class JNISession implements Session
{
    private SWIGTYPE_p_pn_session_t _impl;
    private Object _context;
    private JNIConnection _connection;
    private ErrorCondition _condition = new ErrorCondition();
    private ErrorCondition _remoteCondition = new ErrorCondition();

    JNISession(SWIGTYPE_p_pn_session_t session_t)
    {
        _impl = session_t;
        Proton.pn_session_set_context(_impl, this);
        _connection = JNIConnection.getConnection(Proton.pn_session_connection(_impl));
    }

    @Override
    public int getIncomingCapacity()
    {
        return (int) Proton.pn_session_get_incoming_capacity(_impl);
    }

    @Override
    public void setIncomingCapacity(int capacity)
    {
        Proton.pn_session_set_incoming_capacity(_impl, capacity);
    }

    @Override
    public int getIncomingBytes()
    {
        return (int) Proton.pn_session_incoming_bytes(_impl);
    }

    @Override
    public int getOutgoingBytes()
    {
        return (int) Proton.pn_session_outgoing_bytes(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_sender")
    public Sender sender(String name)
    {
        return new JNISender(Proton.pn_sender(_impl, name));
    }

    @Override
    @ProtonCEquivalent("pn_receiver")
    public Receiver receiver(String name)
    {
        //TODO
        return new JNIReceiver(Proton.pn_receiver(_impl, name));
    }

    @Override
    @ProtonCEquivalent("pn_session_next")
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
    @ProtonCEquivalent("pn_session_connection")
    public Connection getConnection()
    {
        return _connection;
    }

    @Override
    @ProtonCEquivalent("pn_session_state")
    public EndpointState getLocalState()
    {
        return StateConverter.getLocalState(Proton.pn_session_state(_impl));
    }

    @Override
    @ProtonCEquivalent("pn_session_state")
    public EndpointState getRemoteState()
    {
        return StateConverter.getRemoteState(Proton.pn_session_state(_impl));
    }

    @Override
    public ErrorCondition getCondition()
    {
        return _condition;
    }

    @Override
    public void setCondition(ErrorCondition condition)
    {
        if(condition != null)
        {
            _condition.copyFrom(condition);
        }
        else
        {
            _condition.clear();
        }
    }

    @Override
    public ErrorCondition getRemoteCondition()
    {
        SWIGTYPE_p_pn_condition_t cond = Proton.pn_session_remote_condition(_impl);
        _remoteCondition.setCondition(Symbol.valueOf(Proton.pn_condition_get_name(cond)));
        _remoteCondition.setDescription(Proton.pn_condition_get_description(cond));
        JNIData data = new JNIData(Proton.pn_condition_info(cond));
        if(data.next() == Data.DataType.MAP)
        {
            _remoteCondition.setInfo(data.getJavaMap());
        }
        return _remoteCondition;
    }


    @Override
    @ProtonCEquivalent("pn_session_free")
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
    @ProtonCEquivalent("pn_session_open")
    public void open()
    {
        Proton.pn_session_open(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_session_close")
    public void close()
    {
        SWIGTYPE_p_pn_condition_t cond = Proton.pn_session_condition(_impl);
        if(_condition.getCondition() != null)
        {
            Proton.pn_condition_set_name(cond, _condition.getCondition().toString());
            Proton.pn_condition_set_description(cond, _condition.getDescription());
            if(_condition.getInfo() != null && !_condition.getInfo().isEmpty())
            {
                JNIData data = new JNIData(Proton.pn_condition_info(cond));
                data.putJavaMap(_condition.getInfo());
            }
        }

        Proton.pn_session_close(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_session_set_context")
    public void setContext(Object o)
    {
        _context = o;
    }

    @Override
    @ProtonCEquivalent("pn_session_get_context")
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
