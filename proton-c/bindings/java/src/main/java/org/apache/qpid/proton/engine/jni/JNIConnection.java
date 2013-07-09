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

import java.util.ArrayList;
import java.util.EnumSet;

import java.util.List;
import java.util.Map;
import org.apache.qpid.proton.ProtonCEquivalent;
import org.apache.qpid.proton.ProtonUnsupportedOperationException;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.transport.ErrorCondition;
import org.apache.qpid.proton.codec.Data;
import org.apache.qpid.proton.codec.jni.JNIData;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.EngineLogger;
import org.apache.qpid.proton.engine.Link;
import org.apache.qpid.proton.engine.Session;
import org.apache.qpid.proton.jni.Proton;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_condition_t;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_connection_t;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_delivery_t;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_link_t;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_session_t;

public class JNIConnection implements Connection
{
    public static final Symbol[] EMPTY_CAPABILTIES = new Symbol[0];
    private SWIGTYPE_p_pn_connection_t _impl;
    private Object _context;
    private ErrorCondition _condition = new ErrorCondition();
    private ErrorCondition _remoteCondition = new ErrorCondition();


    public JNIConnection()
    {
        this(Proton.pn_connection());
    }

    public JNIConnection(SWIGTYPE_p_pn_connection_t connection_t)
    {
        _impl = connection_t;
        Proton.pn_connection_set_context(_impl, this);
    }



    @Override
    @ProtonCEquivalent("pn_session")
    public Session session()
    {
        return new JNISession(Proton.pn_session(_impl));
    }

    @Override
    @ProtonCEquivalent("pn_session_head")
    public Session sessionHead(EnumSet<EndpointState> local, EnumSet<EndpointState> remote)
    {
        return JNISession.getSession(Proton.pn_session_head(_impl, StateConverter.getStateMask(local, remote)));
    }

    @Override
    @ProtonCEquivalent("pn_link_head")
    public Link linkHead(EnumSet<EndpointState> local, EnumSet<EndpointState> remote)
    {
        return JNILink.getLink(Proton.pn_link_head(_impl, StateConverter.getStateMask(local, remote)));
    }

    @Override
    @ProtonCEquivalent("pn_work_head")
    public Delivery getWorkHead()
    {
        return JNIDelivery.getDelivery(Proton.pn_work_head(_impl));

    }

    @Override
    @ProtonCEquivalent("pn_connection_set_container")
    public void setContainer(String container)
    {
        Proton.pn_connection_set_container(_impl, container);
    }

    @Override
    @ProtonCEquivalent("pn_connection_set_hostname")
    public void setHostname(String hostname)
    {
        Proton.pn_connection_set_hostname(_impl, hostname);
    }

    @Override
    @ProtonCEquivalent("pn_connection_remote_container")
    public String getRemoteContainer()
    {
        return Proton.pn_connection_remote_container(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_connection_remote_hostname")
    public String getRemoteHostname()
    {
        return Proton.pn_connection_remote_hostname(_impl);
    }

    @Override
    public EndpointState getLocalState()
    {
        return StateConverter.getLocalState(Proton.pn_connection_state(_impl));
    }

    @Override
    public EndpointState getRemoteState()
    {
        return StateConverter.getRemoteState(Proton.pn_connection_state(_impl));
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
        SWIGTYPE_p_pn_condition_t cond = Proton.pn_connection_remote_condition(_impl);
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
    @ProtonCEquivalent("pn_connection_free")
    public void free()
    {
        if(_impl != null)
        {
            Proton.pn_connection_set_context(_impl, null);
            Proton.pn_connection_free(_impl);
            _impl = null;
        }
    }

    @Override
    @ProtonCEquivalent("pn_connection_open")
    public void open()
    {
        Proton.pn_connection_open(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_connection_close")
    public void close()
    {
        SWIGTYPE_p_pn_condition_t cond = Proton.pn_connection_condition(_impl);
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
        Proton.pn_connection_close(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_connection_set_context")

    public void setContext(Object o)
    {
        _context = o;
    }

    @Override
    @ProtonCEquivalent("pn_connection_get_context")
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

    SWIGTYPE_p_pn_connection_t getImpl()
    {
        return _impl;
    }

    public static JNIConnection getConnection(SWIGTYPE_p_pn_connection_t connection_t)
    {
        if(connection_t != null)
        {
            JNIConnection connectionObj = (JNIConnection) Proton.pn_connection_get_context(connection_t);
            if(connectionObj == null)
            {
                connectionObj = new JNIConnection(connection_t);
            }
            return connectionObj;
        }
        return null;
    }

    @Override
    @ProtonCEquivalent("pn_connection_offered_capabilities")
    public void setOfferedCapabilities(Symbol[] capabilities)
    {
        JNIData data = new JNIData(Proton.pn_connection_offered_capabilities(_impl));
        data.clear();
        if(capabilities != null)
        {
            data.putJavaArray(capabilities);
        }
    }

    @Override
    @ProtonCEquivalent("pn_connection_desired_capabilities")
    public void setDesiredCapabilities(Symbol[] capabilities)
    {
        JNIData data = new JNIData(Proton.pn_connection_desired_capabilities(_impl));
        data.clear();
        if(capabilities != null)
        {
            data.putJavaArray(capabilities);
        }
    }


    @Override
    @ProtonCEquivalent("pn_connection_remote_offered_capabilities")
    public Symbol[] getRemoteOfferedCapabilities()
    {
        JNIData data = new JNIData(Proton.pn_connection_remote_offered_capabilities(_impl));
        data.rewind();
        if(data.next() != null)
        {
            return (Symbol[]) data.getJavaArray();
        }
        return EMPTY_CAPABILTIES;
    }

    @Override
    @ProtonCEquivalent("pn_connection_remote_desired_capabilities")
    public Symbol[] getRemoteDesiredCapabilities()
    {
        JNIData data = new JNIData(Proton.pn_connection_remote_desired_capabilities(_impl));
        data.rewind();
        if(data.next() != null)
        {
            return (Symbol[]) data.getJavaArray();
        }
        return EMPTY_CAPABILTIES;
    }

    @Override
    public Map<Symbol, Object> getRemoteProperties()
    {
        throw new ProtonUnsupportedOperationException();
    }

    @Override
    public void setProperties(Map<Symbol, Object> properties)
    {
        throw new ProtonUnsupportedOperationException();
    }

    public EngineLogger getEngineLogger()
    {
        //TODO: implement
        return null;
    }

    public void setEngineLogger(EngineLogger engineLogger)
    {
        //TODO: implement
    }
}
