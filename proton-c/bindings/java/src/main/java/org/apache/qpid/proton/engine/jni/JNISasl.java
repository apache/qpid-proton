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
import java.util.EnumMap;
import java.util.HashMap;
import java.util.Map;

import org.apache.qpid.proton.ProtonCEquivalent;
import org.apache.qpid.proton.engine.Sasl;
import org.apache.qpid.proton.jni.Proton;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_sasl_t;
import org.apache.qpid.proton.jni.pn_sasl_outcome_t;
import org.apache.qpid.proton.jni.pn_sasl_state_t;

public class JNISasl implements Sasl
{
    private final SWIGTYPE_p_pn_sasl_t _impl;

    JNISasl(SWIGTYPE_p_pn_sasl_t sasl)
    {
        _impl = sasl;
    }

    @Override
    @ProtonCEquivalent("pn_sasl_state")
    public SaslState getState()
    {
        return convertState(Proton.pn_sasl_state(_impl));
    }

    private static final Map<pn_sasl_state_t, SaslState> _states = new HashMap<pn_sasl_state_t, SaslState>();

    static
    {
        _states.put(pn_sasl_state_t.PN_SASL_CONF, SaslState.PN_SASL_CONF);
        _states.put(pn_sasl_state_t.PN_SASL_FAIL, SaslState.PN_SASL_FAIL);
        _states.put(pn_sasl_state_t.PN_SASL_IDLE, SaslState.PN_SASL_IDLE);
        _states.put(pn_sasl_state_t.PN_SASL_PASS, SaslState.PN_SASL_PASS);
        _states.put(pn_sasl_state_t.PN_SASL_STEP, SaslState.PN_SASL_STEP);
    }

    private SaslState convertState(pn_sasl_state_t pn_sasl_state_t)
    {
        return _states.get(pn_sasl_state_t);
    }

    @Override
    @ProtonCEquivalent("pn_sasl_mechanisms")
    public void setMechanisms(String[] mechanisms)
    {
        StringBuilder build = new StringBuilder();
        for(String mech : mechanisms)
        {
            if(build.length() > 0)
            {
                build.append(' ');
            }
            build.append(mech);
        }
        build.append((char)0);

        Proton.pn_sasl_mechanisms(_impl, build.toString());
    }

    @Override
    @ProtonCEquivalent("pn_sasl_remote_mechanisms")
    public String[] getRemoteMechanisms()
    {
        String mechs = Proton.pn_sasl_remote_mechanisms(_impl);
        return mechs == null ? null : mechs.split(" ");
    }

    @Override
    @ProtonCEquivalent("pn_sasl_pending")
    public int pending()
    {
        return (int) Proton.pn_sasl_pending(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_sasl_recv")
    public int recv(byte[] bytes, int offset, int size)
    {
        return Proton.pn_sasl_recv(_impl, ByteBuffer.wrap(bytes, offset, size));
    }

    @Override
    @ProtonCEquivalent("pn_sasl_send")
    public int send(byte[] bytes, int offset, int size)
    {
        return Proton.pn_sasl_send(_impl, ByteBuffer.wrap(bytes, offset, size));
    }

    @Override
    @ProtonCEquivalent("pn_sasl_done")
    public void done(SaslOutcome outcome)
    {
        Proton.pn_sasl_done(_impl, convertOutcome(outcome));
    }


    private static final EnumMap<SaslOutcome, pn_sasl_outcome_t> _outcomes =
            new EnumMap<SaslOutcome,pn_sasl_outcome_t>(SaslOutcome.class);

    private static final Map<pn_sasl_outcome_t, SaslOutcome> _pn_outcomes =
                new HashMap<pn_sasl_outcome_t, SaslOutcome>();

    private static void mapOutcome(SaslOutcome a, pn_sasl_outcome_t b)
    {
        _outcomes.put(a,b);
        _pn_outcomes.put(b,a);
    }

    static
    {
        mapOutcome(SaslOutcome.PN_SASL_AUTH, pn_sasl_outcome_t.PN_SASL_AUTH);
        mapOutcome(SaslOutcome.PN_SASL_NONE, pn_sasl_outcome_t.PN_SASL_NONE);
        mapOutcome(SaslOutcome.PN_SASL_OK, pn_sasl_outcome_t.PN_SASL_OK);
        mapOutcome(SaslOutcome.PN_SASL_PERM, pn_sasl_outcome_t.PN_SASL_PERM);
        mapOutcome(SaslOutcome.PN_SASL_SYS, pn_sasl_outcome_t.PN_SASL_SYS);
        mapOutcome(SaslOutcome.PN_SASL_TEMP, pn_sasl_outcome_t.PN_SASL_TEMP);
    }

    private static pn_sasl_outcome_t convertOutcome(SaslOutcome outcome)
    {
        return _outcomes.get(outcome);
    }

    @Override
    @ProtonCEquivalent("pn_sasl_plain")

    public void plain(String username, String password)
    {
        Proton.pn_sasl_plain(_impl, username, password);
    }

    @Override
    @ProtonCEquivalent("pn_sasl_outcome")
    public SaslOutcome getOutcome()
    {
        return _pn_outcomes.get(Proton.pn_sasl_outcome(_impl));
    }

    @Override
    @ProtonCEquivalent("pn_sasl_client")
    public void client()
    {
        Proton.pn_sasl_client(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_sasl_server")
    public void server()
    {
        Proton.pn_sasl_server(_impl);
    }
}
