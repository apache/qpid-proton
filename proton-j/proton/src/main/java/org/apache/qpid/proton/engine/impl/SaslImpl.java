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

import static org.apache.qpid.proton.engine.impl.ByteBufferUtils.newReadableBuffer;
import static org.apache.qpid.proton.engine.impl.ByteBufferUtils.newWriteableBuffer;
import static org.apache.qpid.proton.engine.impl.ByteBufferUtils.pour;
import static org.apache.qpid.proton.engine.impl.ByteBufferUtils.pourAll;
import static org.apache.qpid.proton.engine.impl.ByteBufferUtils.pourBufferToArray;

import java.nio.ByteBuffer;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.security.SaslChallenge;
import org.apache.qpid.proton.amqp.security.SaslCode;
import org.apache.qpid.proton.amqp.security.SaslFrameBody;
import org.apache.qpid.proton.amqp.security.SaslInit;
import org.apache.qpid.proton.amqp.security.SaslMechanisms;
import org.apache.qpid.proton.amqp.security.SaslResponse;
import org.apache.qpid.proton.codec.AMQPDefinedTypes;
import org.apache.qpid.proton.codec.DecoderImpl;
import org.apache.qpid.proton.codec.EncoderImpl;
import org.apache.qpid.proton.codec.WritableBuffer;
import org.apache.qpid.proton.engine.Sasl;
import org.apache.qpid.proton.engine.TransportResult;
import org.apache.qpid.proton.engine.TransportResultFactory;

public class SaslImpl implements Sasl, SaslFrameBody.SaslFrameBodyHandler<Void>, SaslFrameHandler
{
    private static final Logger _logger = Logger.getLogger(SaslImpl.class.getName());

    public static final byte SASL_FRAME_TYPE = (byte) 1;

    private final DecoderImpl _decoder = new DecoderImpl();
    private final EncoderImpl _encoder = new EncoderImpl(_decoder);

    private final ByteBuffer _inputBuffer;
    private final ByteBuffer _outputBuffer;
    private final FrameWriter _frameWriter;

    private ByteBuffer _pending;

    private boolean _headerWritten;
    private Binary _challengeResponse;
    private SaslFrameParser _frameParser;
    private boolean _initReceived;
    private boolean _mechanismsSent;
    private boolean _initSent;

    enum Role { CLIENT, SERVER };

    private SaslOutcome _outcome = SaslOutcome.PN_SASL_NONE;
    private SaslState _state = SaslState.PN_SASL_IDLE;

    private String _hostname;
    private boolean _done;
    private Symbol[] _mechanisms;

    private Symbol _chosenMechanism;

    private Role _role;

    /**
     * @param maxFrameSize the size of the input and output buffers
     * returned by {@link SaslTransportWrapper#getInputBuffer()} and
     * {@link SaslTransportWrapper#getOutputBuffer()}.
     */
    SaslImpl(int maxFrameSize)
    {
        _inputBuffer = newWriteableBuffer(maxFrameSize);
        _outputBuffer = newWriteableBuffer(maxFrameSize);

        AMQPDefinedTypes.registerAllTypes(_decoder,_encoder);
        _frameParser = new SaslFrameParser(this, _decoder);
        _frameWriter = new FrameWriter(_encoder, maxFrameSize, FrameWriter.SASL_FRAME_TYPE, null);
    }

    @Override
    public boolean isDone()
    {
        return _done && (_role==Role.CLIENT || _initReceived);
    }

    private void writeSaslOutput()
    {
        process();
        _frameWriter.readBytes(_outputBuffer);

        if(_logger.isLoggable(Level.FINER))
        {
            _logger.log(Level.FINER, "Finished writing SASL output. Output Buffer : " + _outputBuffer);
        }
    }

    private void process()
    {
        processHeader();

        if(_role == Role.SERVER)
        {
            if(!_mechanismsSent && _mechanisms != null)
            {
                SaslMechanisms mechanisms = new SaslMechanisms();

                mechanisms.setSaslServerMechanisms(_mechanisms);
                writeFrame(mechanisms);
                _mechanismsSent = true;
                _state = SaslState.PN_SASL_STEP;
            }

            if(getState() == SaslState.PN_SASL_STEP && getChallengeResponse() != null)
            {
                SaslChallenge challenge = new SaslChallenge();
                challenge.setChallenge(getChallengeResponse());
                writeFrame(challenge);
                setChallengeResponse(null);
            }

            if(_done)
            {
                org.apache.qpid.proton.amqp.security.SaslOutcome outcome =
                        new org.apache.qpid.proton.amqp.security.SaslOutcome();
                outcome.setCode(SaslCode.values()[_outcome.getCode()]);
                writeFrame(outcome);
            }
        }
        else if(_role == Role.CLIENT)
        {
            if(getState() == SaslState.PN_SASL_IDLE && _chosenMechanism != null)
            {
                processInit();
                _state = SaslState.PN_SASL_STEP;

                //HACK: if we received an outcome before
                //we sent our init, change the state now
                if(_outcome != SaslOutcome.PN_SASL_NONE)
                {
                    _state = classifyStateFromOutcome(_outcome);
                }
            }

            if(getState() == SaslState.PN_SASL_STEP && getChallengeResponse() != null)
            {
                processResponse();
            }

        }
    }

    private void writeFrame(SaslFrameBody frameBody)
    {
        _frameWriter.writeFrame(frameBody);
    }

    @Override
    final public int recv(byte[] bytes, int offset, int size)
    {
        if(_pending == null)
        {
            return -1;
        }
        final int written = pourBufferToArray(_pending, bytes, offset, size);
        if(!_pending.hasRemaining())
        {
            _pending = null;
        }
        return written;
    }

    @Override
    final public int send(byte[] bytes, int offset, int size)
    {
        byte[] data = new byte[size];
        System.arraycopy(bytes, offset, data, 0, size);
        setChallengeResponse(new Binary(data));
        return size;
    }

    final int processHeader()
    {

        if(!_headerWritten)
        {
            _frameWriter.writeHeader(AmqpHeader.SASL_HEADER);

            _headerWritten = true;
            return AmqpHeader.SASL_HEADER.length;
        }
        else
        {
            return 0;
        }
    }

    @Override
    public int pending()
    {
        return _pending == null ? 0 : _pending.remaining();
    }

    void setPending(ByteBuffer pending)
    {
        _pending = pending;
    }

    @Override
    public SaslState getState()
    {
        return _state;
    }

    final Binary getChallengeResponse()
    {
        return _challengeResponse;
    }

    final void setChallengeResponse(Binary challengeResponse)
    {
        _challengeResponse = challengeResponse;
    }

    @Override
    public void setMechanisms(String... mechanisms)
    {
        if(mechanisms != null)
        {
            _mechanisms = new Symbol[mechanisms.length];
            for(int i = 0; i < mechanisms.length; i++)
            {
                _mechanisms[i] = Symbol.valueOf(mechanisms[i]);
            }
        }

        if(_role == Role.CLIENT)
        {
            assert mechanisms != null;
            assert mechanisms.length == 1;

            _chosenMechanism = Symbol.valueOf(mechanisms[0]);
        }
    }

    @Override
    public String[] getRemoteMechanisms()
    {
        if(_role == Role.SERVER)
        {
            return _chosenMechanism == null ? new String[0] : new String[] { _chosenMechanism.toString() };
        }
        else if(_role == Role.CLIENT)
        {
            if(_mechanisms == null)
            {
                return new String[0];
            }
            else
            {
                String[] remoteMechanisms = new String[_mechanisms.length];
                for(int i = 0; i < _mechanisms.length; i++)
                {
                    remoteMechanisms[i] = _mechanisms[i].toString();
                }
                return remoteMechanisms;
            }
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

    public Symbol getChosenMechanism()
    {
        return _chosenMechanism;
    }

    public void setResponse(Binary initialResponse)
    {
        setPending(initialResponse.asByteBuffer());
    }

    @Override
    public void handle(SaslFrameBody frameBody, Binary payload)
    {
        frameBody.invoke(this, payload, null);
    }

    @Override
    public void handleInit(SaslInit saslInit, Binary payload, Void context)
    {
        if(_role == null)
        {
            server();
        }
        checkRole(Role.SERVER);
        _hostname = saslInit.getHostname();
        _chosenMechanism = saslInit.getMechanism();
        _initReceived = true;
        if(saslInit.getInitialResponse() != null)
        {
            setPending(saslInit.getInitialResponse().asByteBuffer());

        }
    }

    @Override
    public void handleResponse(SaslResponse saslResponse, Binary payload, Void context)
    {
        checkRole(Role.SERVER);
        setPending(saslResponse.getResponse()  == null ? null : saslResponse.getResponse().asByteBuffer());
    }

    @Override
    public void done(SaslOutcome outcome)
    {
        checkRole(Role.SERVER);
        _outcome = outcome;
        _done = true;
        _state = classifyStateFromOutcome(outcome);
        _logger.fine("SASL negotiation done: " + this);
    }

    private void checkRole(Role role)
    {
        if(role != _role)
        {
            throw new IllegalStateException("Role is " + _role + " but should be " + role);
        }
    }

    @Override
    public void handleMechanisms(SaslMechanisms saslMechanisms, Binary payload, Void context)
    {
        if(_role == null)
        {
            client();
        }
        checkRole(Role.CLIENT);
        _mechanisms = saslMechanisms.getSaslServerMechanisms();
    }

    @Override
    public void handleChallenge(SaslChallenge saslChallenge, Binary payload, Void context)
    {
        checkRole(Role.CLIENT);
        setPending(saslChallenge.getChallenge()  == null ? null : saslChallenge.getChallenge().asByteBuffer());
    }

    @Override
    public void handleOutcome(org.apache.qpid.proton.amqp.security.SaslOutcome saslOutcome,
                              Binary payload,
                              Void context)
    {
        checkRole(Role.CLIENT);
        for(SaslOutcome outcome : SaslOutcome.values())
        {
            if(outcome.getCode() == saslOutcome.getCode().ordinal())
            {
                _outcome = outcome;
                if (_state != SaslState.PN_SASL_IDLE)
                {
                    _state = classifyStateFromOutcome(outcome);
                }
                break;
            }
        }
        _done = true;

        if(_logger.isLoggable(Level.FINE))
        {
            _logger.fine("Handled outcome: " + this);
        }
    }

    private SaslState classifyStateFromOutcome(SaslOutcome outcome)
    {
        return outcome == SaslOutcome.PN_SASL_OK ? SaslState.PN_SASL_PASS : SaslState.PN_SASL_FAIL;
    }

    private void processResponse()
    {
        SaslResponse response = new SaslResponse();
        response.setResponse(getChallengeResponse());
        setChallengeResponse(null);
        writeFrame(response);
    }

    private void processInit()
    {
        SaslInit init = new SaslInit();
        init.setHostname(_hostname);
        init.setMechanism(_chosenMechanism);
        if(getChallengeResponse() != null)
        {
            init.setInitialResponse(getChallengeResponse());
            setChallengeResponse(null);
        }
        _initSent = true;
        writeFrame(init);
    }

    @Override
    public void plain(String username, String password)
    {
        client();
        _chosenMechanism = Symbol.valueOf("PLAIN");
        byte[] usernameBytes = username.getBytes();
        byte[] passwordBytes = password.getBytes();
        byte[] data = new byte[usernameBytes.length+passwordBytes.length+2];
        System.arraycopy(usernameBytes, 0, data, 1, usernameBytes.length);
        System.arraycopy(passwordBytes, 0, data, 2+usernameBytes.length, passwordBytes.length);

        setChallengeResponse(new Binary(data));

    }

    @Override
    public SaslOutcome getOutcome()
    {
        return _outcome;
    }

    @Override
    public void client()
    {
        _role = Role.CLIENT;
        if(_mechanisms != null)
        {
            assert _mechanisms.length == 1;

            _chosenMechanism = _mechanisms[0];
        }
    }

    @Override
    public void server()
    {
        _role = Role.SERVER;
    }


    public TransportWrapper wrap(final TransportInput input, final TransportOutput output)
    {
        return new SaslTransportWrapper(input, output);
    }

    @Override
    public String toString()
    {
        StringBuilder builder = new StringBuilder();
        builder
            .append("SaslImpl [_outcome=").append(_outcome)
            .append(", state=").append(_state)
            .append(", done=").append(_done)
            .append(", role=").append(_role)
            .append("]");
        return builder.toString();
    }


    private class SaslTransportWrapper implements TransportWrapper
    {
        private final TransportInput _underlyingInput;
        private final TransportOutput _underlyingOutput;
        private boolean _outputComplete;

        /**
         * We store the last result when processing input so that
         * we know not to process any more input if it was an error.
         */
        private TransportResult _lastInputResult = TransportResultFactory.ok();
        private ByteBuffer _readOnlyOutputBufferView;

        private SaslTransportWrapper(TransportInput input, TransportOutput output)
        {
            _underlyingInput = input;
            _underlyingOutput = output;
        }

        private void fillOutputBuffer()
        {
            if(isOutputInSaslMode())
            {
                SaslImpl.this.writeSaslOutput();
                if(_done)
                {
                    _outputComplete = true;
                }

                // TODO if sasl is now 'done', it would be more efficient if any remaining space is
                // now offered directly to _output rather than awaiting the next invocation.
            }
            else
            {
                ByteBuffer outputBuffer = _underlyingOutput.getOutputBuffer();
                pour(outputBuffer, _outputBuffer);

                _underlyingOutput.outputConsumed();

                if(_logger.isLoggable(Level.FINER))
                {
                    _logger.log(Level.FINER, SaslImpl.this + " filled output buffer with plain output");
                }
            }
        }

        /**
         * TODO rationalise this method with respect to the other similar checks of _role/_initReceived etc
         * @see SaslImpl#isDone()
         */
        private boolean isInputInSaslMode()
        {
            return _role == null || (_role == Role.CLIENT && !_done) ||(_role == Role.SERVER && (!_initReceived || !_done));
        }

        private boolean isOutputInSaslMode()
        {
            return _role == null || (_role == Role.CLIENT && (!_done || !_initSent)) || (_role == Role.SERVER && !_outputComplete);
        }

        @Override
        public ByteBuffer getInputBuffer()
        {
            // TODO if the SASL negotiation is complete, it would be more efficient
            // to return the underlying transport input's buffer.
            // The same optimisation would be possible in getOutputBuffer
            _lastInputResult.checkIsOk();
            return _inputBuffer;
        }

        @Override
        public TransportResult processInput()
        {
            _inputBuffer.flip();

            try
            {
                _lastInputResult = reallyProcessInput();
                return _lastInputResult;
            }
            finally
            {
                _inputBuffer.compact();
            }
        }

        private TransportResult reallyProcessInput()
        {
            TransportResult transportResult = TransportResultFactory.ok();

            if(isInputInSaslMode())
            {
                if(_logger.isLoggable(Level.FINER))
                {
                    _logger.log(Level.FINER, SaslImpl.this + " about to call input.");
                }

                transportResult = _frameParser.input(_inputBuffer);
            }

            if(!isInputInSaslMode() && transportResult.isOk())
            {
                if(_logger.isLoggable(Level.FINER))
                {
                    _logger.log(Level.FINER, SaslImpl.this + " about to call plain input");
                }

                transportResult = pourAll(_inputBuffer, _underlyingInput);
            }
            return transportResult;
        }

        @Override
        public ByteBuffer getOutputBuffer()
        {
            fillOutputBuffer();
            _outputBuffer.flip();

            _readOnlyOutputBufferView = _outputBuffer.asReadOnlyBuffer();
            return _readOnlyOutputBufferView;
        }

        @Override
        public void outputConsumed()
        {
            _outputBuffer.position(_readOnlyOutputBufferView.position());
            _outputBuffer.compact();
        }

    }
}
