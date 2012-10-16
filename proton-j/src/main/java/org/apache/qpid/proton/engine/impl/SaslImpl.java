package org.apache.qpid.proton.engine.impl;

import java.nio.ByteBuffer;
import org.apache.qpid.proton.codec.CompositeWritableBuffer;
import org.apache.qpid.proton.codec.DecoderImpl;
import org.apache.qpid.proton.codec.EncoderImpl;
import org.apache.qpid.proton.codec.WritableBuffer;
import org.apache.qpid.proton.engine.Sasl;
import org.apache.qpid.proton.type.AMQPDefinedTypes;
import org.apache.qpid.proton.type.Binary;
import org.apache.qpid.proton.type.Symbol;
import org.apache.qpid.proton.type.UnsignedByte;
import org.apache.qpid.proton.type.security.*;

public class SaslImpl implements Sasl, SaslFrameBody.SaslFrameBodyHandler<Void>
{
    public static final byte SASL_FRAME_TYPE = (byte) 1;

    public static final byte[] HEADER =
            new byte[] { (byte) 'A',
                         (byte) 'M',
                         (byte) 'Q',
                         (byte) 'P',
                         3,
                         1,
                         0,
                         0
                       };

    private ByteBuffer _pending;
    private final DecoderImpl _decoder = new DecoderImpl();
    private final EncoderImpl _encoder = new EncoderImpl(_decoder);
    private int _maxFrameSize = 4096;
    private final ByteBuffer _overflowBuffer = ByteBuffer.wrap(new byte[_maxFrameSize]);
    private boolean _headerWritten;
    private Binary _challengeResponse;
    private SaslFrameParser _frameParser;

    enum Role { CLIENT, SERVER };

    private SaslOutcome _outcome = SaslOutcome.PN_SASL_NONE;
    private SaslState _state = SaslState.PN_SASL_IDLE;

    private String _hostname;
    private boolean _done;
    private Symbol[] _mechanisms;

    private Symbol _chosenMechanism;


    private Role _role;

    public SaslImpl()
    {
        _frameParser = new SaslFrameParser(this);
        AMQPDefinedTypes.registerAllTypes(_decoder);
        _overflowBuffer.flip();
    }

    public boolean isDone()
    {
        return _done;
    }

    public final int input(byte[] bytes, int offset, int size)
    {
        if(isDone())
        {
            return TransportImpl.END_OF_STREAM;
        }
        else
        {
            return getFrameParser().input(bytes, offset, size);
        }
    }

    public final int output(byte[] bytes, int offset, int size)
    {

        int written = 0;
        if(_overflowBuffer.hasRemaining())
        {
            final int overflowWritten = Math.min(size, _overflowBuffer.remaining());
            _overflowBuffer.get(bytes, offset, overflowWritten);
            written+=overflowWritten;
        }
        if(!_overflowBuffer.hasRemaining())
        {
            _overflowBuffer.clear();

            CompositeWritableBuffer outputBuffer =
                    new CompositeWritableBuffer(
                       new WritableBuffer.ByteBufferWrapper(ByteBuffer.wrap(bytes, offset + written, size - written)),
                       new WritableBuffer.ByteBufferWrapper(_overflowBuffer));


            written += process(outputBuffer);
        }
        return written;
    }


    protected int process(WritableBuffer buffer)
    {
        int written = processHeader(buffer);

        if(_role == Role.SERVER)
        {

            if(getState()== SaslState.PN_SASL_IDLE && _mechanisms != null)
            {
                SaslMechanisms mechanisms = new SaslMechanisms();

                mechanisms.setSaslServerMechanisms(_mechanisms);
                written += writeFrame(buffer, mechanisms);
            }
            else if(getChallengeResponse() != null)
            {
                SaslChallenge challenge = new SaslChallenge();
                challenge.setChallenge(getChallengeResponse());
                written+=writeFrame(buffer, challenge);
                setChallengeResponse(null);
            }
            else if(_done)
            {
                org.apache.qpid.proton.type.security.SaslOutcome outcome =
                        new org.apache.qpid.proton.type.security.SaslOutcome();
                outcome.setCode(UnsignedByte.valueOf(_outcome.getCode()));
                written+=writeFrame(buffer, outcome);
            }
            return written;
        }
        else if(_role == Role.CLIENT)
        {
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
        else
        {
            throw new IllegalStateException("Client or server must be chosen");
        }
    }

    int writeFrame(WritableBuffer buffer, SaslFrameBody frameBody)
    {
        int oldPosition = buffer.position();
        buffer.position(buffer.position()+8);
        _encoder.setByteBuffer(buffer);
        _encoder.writeDescribedType(frameBody);

        int frameSize = buffer.position() - oldPosition;
        int limit = buffer.position();
        buffer.position(oldPosition);
        buffer.putInt(frameSize);
        buffer.put((byte) 2);
        buffer.put(SASL_FRAME_TYPE);
        buffer.putShort((short) 0);
        buffer.position(limit);

        return frameSize;
    }

    final public int recv(byte[] bytes, int offset, int size)
    {
        final int written = Math.min(size, _pending.remaining());
        _pending.get(bytes, offset, written);
        if(!_pending.hasRemaining())
        {
            _pending = null;
        }
        return written;
    }

    final public int send(byte[] bytes, int offset, int size)
    {
        byte[] data = new byte[size];
        System.arraycopy(bytes, offset, data, 0, size);
        setChallengeResponse(new Binary(data));
        return size;
    }

    final int processHeader(WritableBuffer outputBuffer)
    {

        if(!_headerWritten)
        {
            outputBuffer.put(HEADER,0, HEADER.length);

            _headerWritten = true;
            return HEADER.length;
        }
        else
        {
            return 0;
        }
    }

    public int pending()
    {
        return _pending == null ? 0 : _pending.remaining();
    }

    void setPending(ByteBuffer pending)
    {
        _pending = pending;
    }

    public SaslState getState()
    {
        return _state;
    }


    final DecoderImpl getDecoder()
    {
        return _decoder;
    }

    final Binary getChallengeResponse()
    {
        return _challengeResponse;
    }

    final void setChallengeResponse(Binary challengeResponse)
    {
        _challengeResponse = challengeResponse;
    }

    final SaslFrameParser getFrameParser()
    {
        return _frameParser;
    }



    public void setMechanisms(String[] mechanisms)
    {
        if(_role == Role.SERVER)
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
        else if(_role == Role.CLIENT)
        {
            assert mechanisms != null;
            assert mechanisms.length == 1;

            _chosenMechanism = Symbol.valueOf(mechanisms[0]);
        }
    }

    public String[] getRemoteMechanisms()
    {
        if(_role == Role.SERVER)
        {
            return new String[] { _chosenMechanism.toString() };
        }
        else if(_role == Role.CLIENT)
        {
            String[] remoteMechanisms = new String[_mechanisms.length];
            for(int i = 0; i < _mechanisms.length; i++)
            {
                remoteMechanisms[i] = _mechanisms[i].toString();
            }
            return remoteMechanisms;
        }
        else
        {
            throw new IllegalStateException();
        }
    }

    public void setMechanism(Symbol mechanism)
    {
        _chosenMechanism = mechanism;
    }

    public void setResponse(Binary initialResponse)
    {
        setPending(initialResponse.asByteBuffer());
    }


    public void handleInit(SaslInit saslInit, Binary payload, Void context)
    {
        _hostname = saslInit.getHostname();
        if(saslInit.getInitialResponse() != null)
        {
            setPending(saslInit.getInitialResponse().asByteBuffer());
        }
    }


    public void handleResponse(SaslResponse saslResponse, Binary payload, Void context)
    {
        setPending(saslResponse.getResponse()  == null ? null : saslResponse.getResponse().asByteBuffer());
    }


    public void done(SaslOutcome outcome)
    {
        checkRole(Role.SERVER);
        _outcome = outcome;
        _done = true;
        _state = outcome == SaslOutcome.PN_SASL_OK ? SaslState.PN_SASL_PASS : SaslState.PN_SASL_FAIL;
    }

    private void checkRole(Role role)
    {
        if(role != _role)
        {
            throw new IllegalStateException();
        }
    }


    public void handleMechanisms(SaslMechanisms saslMechanisms, Binary payload, Void context)
    {
        _mechanisms = saslMechanisms.getSaslServerMechanisms();
    }


    public void handleChallenge(SaslChallenge saslChallenge, Binary payload, Void context)
    {
        setPending(saslChallenge.getChallenge()  == null ? null : saslChallenge.getChallenge().asByteBuffer());
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

    public void client()
    {
        _role = Role.CLIENT;
    }

    public void server()
    {
        _role = Role.SERVER;
    }
}
