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
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.jni.Proton;

class StateConverter
{
    public static EndpointState getLocalState(int state)
    {
        state =  state & Proton.PN_LOCAL_MASK;
        if(state == Proton.PN_LOCAL_UNINIT)
        {
            return EndpointState.UNINITIALIZED;
        }
        else if(state == Proton.PN_LOCAL_ACTIVE)
        {
            return EndpointState.ACTIVE;
        }
        else if(state == Proton.PN_LOCAL_CLOSED)
        {
            return EndpointState.CLOSED;
        }
        //TODO
        return null;
    }


    public static EndpointState getRemoteState(int state)
    {
        state =  state & Proton.PN_REMOTE_MASK;
        if(state == Proton.PN_REMOTE_UNINIT)
        {
            return EndpointState.UNINITIALIZED;
        }
        else if(state == Proton.PN_REMOTE_ACTIVE)
        {
            return EndpointState.ACTIVE;
        }
        else if(state == Proton.PN_REMOTE_CLOSED)
        {
            return EndpointState.CLOSED;
        }
        //TODO
        return null;
    }

    public static int getStateMask(EnumSet<EndpointState> local, EnumSet<EndpointState> remote)
    {
        int state = 0;
        if(local.contains(EndpointState.UNINITIALIZED))
        {
            state &= Proton.PN_LOCAL_UNINIT;
        }
        if(local.contains(EndpointState.ACTIVE))
        {
            state &= Proton.PN_LOCAL_ACTIVE;
        }
        if(local.contains(EndpointState.CLOSED))
        {
            state &= Proton.PN_LOCAL_CLOSED;
        }
        if(remote.contains(EndpointState.UNINITIALIZED))
        {
            state &= Proton.PN_REMOTE_UNINIT;
        }
        if(remote.contains(EndpointState.ACTIVE))
        {
            state &= Proton.PN_REMOTE_ACTIVE;
        }
        if(remote.contains(EndpointState.CLOSED))
        {
            state &= Proton.PN_REMOTE_CLOSED;
        }

        return state;
    }
}
