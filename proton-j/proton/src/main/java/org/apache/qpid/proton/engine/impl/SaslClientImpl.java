package org.apache.qpid.proton.engine.impl;
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


import org.apache.qpid.proton.codec.WritableBuffer;
import org.apache.qpid.proton.engine.SaslClient;
import org.apache.qpid.proton.type.Binary;
import org.apache.qpid.proton.type.Symbol;
import org.apache.qpid.proton.type.security.*;

public class SaslClientImpl extends SaslImpl implements SaslClient, SaslFrameBody.SaslFrameBodyHandler<Void>
{

    private SaslOutcome _outcome = SaslOutcome.PN_SASL_NONE;
    private SaslState _state = SaslState.PN_SASL_IDLE;
    private Symbol[] _mechanisms;
    private Symbol _chosenMechanism;
    private boolean _done;
    private String _hostname;

    public SaslClientImpl()
    {
        super();

    }


    public SaslState getState()
    {
        return _state;
    }

    public void setMechanisms(String[] mechanisms)
    {
        assert mechanisms != null;
        assert mechanisms.length == 1;

        _chosenMechanism = Symbol.valueOf(mechanisms[0]);
    }

    public String[] getRemoteMechanisms()
    {
        String[] remoteMechanisms = new String[_mechanisms.length];
        for(int i = 0; i < _mechanisms.length; i++)
        {
            remoteMechanisms[i] = _mechanisms[i].toString();
        }
        return remoteMechanisms;
    }

    public void handleMechanisms(SaslMechanisms saslMechanisms, Binary payload, Void context)
    {
        _mechanisms = saslMechanisms.getSaslServerMechanisms();

    }

    public void handleInit(SaslInit saslInit, Binary payload, Void context)
    {
        // TODO - error
    }

    public void handleChallenge(SaslChallenge saslChallenge, Binary payload, Void context)
    {
        setPending(saslChallenge.getChallenge()  == null ? null : saslChallenge.getChallenge().asByteBuffer());
    }

    public void handleResponse(SaslResponse saslResponse, Binary payload, Void context)
    {
        // TODO - error
    }

    public void handleOutcome(org.apache.qpid.proton.type.security.SaslOutcome saslOutcome,
                              Binary payload,
                              Void context)
    {
        for(SaslOutcome outcome : SaslOutcome.values())
        {
            if(outcome.getCode() == saslOutcome.getCode().byteValue())
            {
                _outcome = outcome;
                break;
            }
        }
        _done = true;
    }

    @Override
    public boolean isDone()
    {
        return _done;
    }

    @Override
    protected int process(WritableBuffer buffer)
    {
        int written = processHeader(buffer);

        if(getState() == SaslState.PN_SASL_IDLE && _chosenMechanism != null)
        {
            written += processInit(buffer);
            _state = SaslState.PN_SASL_STEP;
        }
        if(getState() == SaslState.PN_SASL_STEP && getChallengeResponse() != null)
        {
            written += processResponse(buffer);
        }
        return written;
    }

    private int processResponse(WritableBuffer buffer)
    {
        SaslResponse response = new SaslResponse();
        response.setResponse(getChallengeResponse());
        setChallengeResponse(null);
        return writeFrame(buffer, response);
    }

    private int processInit(WritableBuffer buffer)
    {
        SaslInit init = new SaslInit();
        init.setHostname(_hostname);
        init.setMechanism(_chosenMechanism);
        if(getChallengeResponse() != null)
        {
            init.setInitialResponse(getChallengeResponse());
            setChallengeResponse(null);
        }
        return writeFrame(buffer, init);
    }

    public void plain(String username, String password)
    {
        _chosenMechanism = Symbol.valueOf("PLAIN");
        byte[] usernameBytes = username.getBytes();
        byte[] passwordBytes = password.getBytes();
        byte[] data = new byte[usernameBytes.length+passwordBytes.length+2];
        System.arraycopy(usernameBytes, 0, data, 1, usernameBytes.length);
        System.arraycopy(passwordBytes, 0, data, 2+usernameBytes.length, passwordBytes.length);

        setChallengeResponse(new Binary(data));

    }

    public SaslOutcome getOutcome()
    {
        return _outcome;
    }
}
