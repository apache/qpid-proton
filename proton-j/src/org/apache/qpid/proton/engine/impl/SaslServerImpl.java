package org.apache.qpid.proton.engine.impl;

import org.apache.qpid.proton.codec.WritableBuffer;
import org.apache.qpid.proton.engine.SaslServer;
import org.apache.qpid.proton.type.Binary;
import org.apache.qpid.proton.type.Symbol;
import org.apache.qpid.proton.type.UnsignedByte;
import org.apache.qpid.proton.type.security.*;

public class SaslServerImpl extends SaslImpl implements SaslServer, SaslFrameBody.SaslFrameBodyHandler<Void>
{

    private static final byte SASL_FRAME_TYPE = (byte) 1;
    private SaslOutcome _outcome = SaslOutcome.PN_SASL_NONE;
    private SaslState _state = SaslState.PN_SASL_IDLE;

    private String _hostname;
    private String _chosenMechanism;
    private boolean _done;
    private Symbol[] _mechanisms;

    public SaslServerImpl()
    {
    }

    public void done(SaslOutcome outcome)
    {
        _outcome = outcome;
        _done = true;
        _state = outcome == SaslOutcome.PN_SASL_OK ? SaslState.PN_SASL_PASS : SaslState.PN_SASL_FAIL;
    }

    protected int process(WritableBuffer outputBuffer)
    {
        int written = processHeader(outputBuffer);

        if(getState()== SaslState.PN_SASL_IDLE && _mechanisms != null)
        {
            SaslMechanisms mechanisms = new SaslMechanisms();

            mechanisms.setSaslServerMechanisms(_mechanisms);
            written += writeFrame(outputBuffer, mechanisms);
        }
        else if(getChallengeResponse() != null)
        {
            SaslChallenge challenge = new SaslChallenge();
            challenge.setChallenge(getChallengeResponse());
            written+=writeFrame(outputBuffer, challenge);
            setChallengeResponse(null);
        }
        else if(_done)
        {
            org.apache.qpid.proton.type.security.SaslOutcome outcome =
                    new org.apache.qpid.proton.type.security.SaslOutcome();
            outcome.setCode(UnsignedByte.valueOf(_outcome.getCode()));
            written+=writeFrame(outputBuffer, outcome);
        }
        return written;
    }


    public SaslState getState()
    {
        return _state;
    }

    public void setMechanisms(String[] mechanisms)
    {
        if(mechanisms != null)
        {
            _mechanisms = new Symbol[mechanisms.length];
            for(int i = 0; i < mechanisms.length; i++)
            {
                _mechanisms[i] = Symbol.valueOf(mechanisms[i]);
            }
        }
    }

    public String[] getRemoteMechanisms()
    {
        return new String[] { _chosenMechanism };
    }

    public void setMechanism(Symbol mechanism)
    {
        _chosenMechanism = mechanism.toString();
    }

    public void setResponse(Binary initialResponse)
    {
        setPending(initialResponse.asByteBuffer());
    }



    public void handleMechanisms(SaslMechanisms saslMechanisms, Binary payload, Void context)
    {
        //TODO - Implement
        // error - should only be sent server -> client
    }

    public void handleInit(SaslInit saslInit, Binary payload, Void context)
    {
        _hostname = saslInit.getHostname();
        if(saslInit.getInitialResponse() != null)
        {
            setPending(saslInit.getInitialResponse().asByteBuffer());
        }
    }

    public void handleChallenge(SaslChallenge saslChallenge, Binary payload, Void context)
    {
        //TODO - Implement
        // error - should only be sent server -> client
    }

    public void handleResponse(SaslResponse saslResponse, Binary payload, Void context)
    {
        setPending(saslResponse.getResponse()  == null ? null : saslResponse.getResponse().asByteBuffer());
    }

    public void handleOutcome(org.apache.qpid.proton.type.security.SaslOutcome saslOutcome,
                              Binary payload,
                              Void context)
    {
        //TODO - Implement
        // error - should only be sent server -> client
    }

    @Override
    public boolean isDone()
    {
        return _done;
    }

}
