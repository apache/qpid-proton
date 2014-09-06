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

import org.apache.qpid.proton.amqp.transport.ErrorCondition;
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.Event;
import org.apache.qpid.proton.engine.ProtonJEndpoint;

public abstract class EndpointImpl implements ProtonJEndpoint
{
    private EndpointState _localState = EndpointState.UNINITIALIZED;
    private EndpointState _remoteState = EndpointState.UNINITIALIZED;
    private ErrorCondition _localError = new ErrorCondition();
    private ErrorCondition _remoteError = new ErrorCondition();
    private boolean _modified;
    private EndpointImpl _transportNext;
    private EndpointImpl _transportPrev;
    private Object _context;

    private int refcount = 1;
    boolean freed = false;

    void incref() {
        refcount++;
    }

    void decref() {
        refcount--;
        if (refcount == 0) {
            postFinal();
        } else if (refcount < 0) {
            throw new IllegalStateException();
        }
    }

    abstract void postFinal();

    abstract void localOpen();

    abstract void localClose();

    public void open()
    {
        switch(_localState)
        {
            case ACTIVE:
                // TODO
            case CLOSED:
                // TODO
            case UNINITIALIZED:
                _localState = EndpointState.ACTIVE;
                localOpen();
        }
        modified();
    }

    public void close()
    {

        switch(_localState)
        {
            case UNINITIALIZED:
                // TODO
            case CLOSED:
                // TODO
            case ACTIVE:
                _localState = EndpointState.CLOSED;
                localClose();
        }
        modified();
    }

    public EndpointState getLocalState()
    {
        return _localState;
    }

    public EndpointState getRemoteState()
    {
        return _remoteState;
    }

    public ErrorCondition getCondition()
    {
        return _localError;
    }

    @Override
    public void setCondition(ErrorCondition condition)
    {
        if(condition != null)
        {
            _localError.copyFrom(condition);
        }
        else
        {
            _localError.clear();
        }
    }

    public ErrorCondition getRemoteCondition()
    {
        return _remoteError;
    }

    void setLocalState(EndpointState localState)
    {
        _localState = localState;
    }

    void setRemoteState(EndpointState remoteState)
    {
        // TODO - check state change legal
        _remoteState = remoteState;
    }

    void modified()
    {
        modified(true);
    }

    void modified(boolean emit)
    {
        if(!_modified)
        {
            _modified = true;
            getConnectionImpl().addModified(this);
        }

        if (emit) {
            ConnectionImpl conn = getConnectionImpl();
            TransportImpl trans = conn.getTransport();
            if (trans != null) {
                conn.put(Event.Type.TRANSPORT, trans);
            }
        }
    }

    protected abstract ConnectionImpl getConnectionImpl();

    void clearModified()
    {
        if(_modified)
        {
            _modified = false;
            getConnectionImpl().removeModified(this);
        }
    }

    boolean isModified()
    {
        return _modified;
    }

    EndpointImpl transportNext()
    {
        return _transportNext;
    }

    EndpointImpl transportPrev()
    {
        return _transportPrev;
    }

    abstract void doFree();

    final public void free()
    {
        if (freed) return;
        freed = true;

        doFree();
        decref();
    }

    void setTransportNext(EndpointImpl transportNext)
    {
        _transportNext = transportNext;
    }

    void setTransportPrev(EndpointImpl transportPrevious)
    {
        _transportPrev = transportPrevious;
    }

    public Object getContext()
    {
        return _context;
    }

    public void setContext(Object context)
    {
        _context = context;
    }

}
