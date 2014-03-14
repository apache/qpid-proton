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

import java.nio.ByteBuffer;

import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.security.SaslFrameBody;
import org.apache.qpid.proton.codec.ByteBufferDecoder;
import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.engine.TransportException;

class SaslFrameParser
{
    private SaslFrameHandler _sasl;

    enum State
    {
        SIZE_0,
        SIZE_1,
        SIZE_2,
        SIZE_3,
        PRE_PARSE,
        BUFFERING,
        PARSING,
        ERROR
    }

    private State _state = State.SIZE_0;
    private int _size;

    private ByteBuffer _buffer;

    private int _ignore = 8;
    private final ByteBufferDecoder _decoder;


    SaslFrameParser(SaslFrameHandler sasl, ByteBufferDecoder decoder)
    {
        _sasl = sasl;
        _decoder = decoder;
    }

    /**
     * Parse the provided SASL input and call my SASL frame handler with the result
     */
    public void input(ByteBuffer input) throws TransportException
    {
        TransportException frameParsingError = null;
        int size = _size;
        State state = _state;
        ByteBuffer oldIn = null;

        // Note that we simply skip over the header rather than parsing it.
        if(_ignore != 0)
        {
            int bytesToEat = Math.min(_ignore, input.remaining());
            input.position(input.position() + bytesToEat);
            _ignore -= bytesToEat;
        }

        while(input.hasRemaining() && state != State.ERROR && !_sasl.isDone())
        {
            switch(state)
            {
                case SIZE_0:
                    if(input.remaining() >= 4)
                    {
                        size = input.getInt();
                        state = State.PRE_PARSE;
                        break;
                    }
                    else
                    {
                        size = (input.get() << 24) & 0xFF000000;
                        if(!input.hasRemaining())
                        {
                            state = State.SIZE_1;
                            break;
                        }
                    }
                case SIZE_1:
                    size |= (input.get() << 16) & 0xFF0000;
                    if(!input.hasRemaining())
                    {
                        state = State.SIZE_2;
                        break;
                    }
                case SIZE_2:
                    size |= (input.get() << 8) & 0xFF00;
                    if(!input.hasRemaining())
                    {
                        state = State.SIZE_3;
                        break;
                    }
                case SIZE_3:
                    size |= input.get() & 0xFF;
                    state = State.PRE_PARSE;

                case PRE_PARSE:
                    if(size < 8)
                    {
                        frameParsingError = new TransportException("specified frame size %d smaller than minimum frame header "
                                                                   + "size %d",
                                                                   _size, 8);
                        state = State.ERROR;
                        break;
                    }

                    if(input.remaining() < size-4)
                    {
                        _buffer = ByteBuffer.allocate(size-4);
                        _buffer.put(input);
                        state = State.BUFFERING;
                        break;
                    }
                case BUFFERING:
                    if(_buffer != null)
                    {
                        if(input.remaining() < _buffer.remaining())
                        {
                            _buffer.put(input);
                            break;
                        }
                        else
                        {
                            ByteBuffer dup = input.duplicate();
                            dup.limit(dup.position()+_buffer.remaining());
                            input.position(input.position()+_buffer.remaining());
                            _buffer.put(dup);
                            oldIn = input;
                            _buffer.flip();
                            input = _buffer;
                            state = State.PARSING;
                        }
                    }

                case PARSING:

                    int dataOffset = (input.get() << 2) & 0x3FF;

                    if(dataOffset < 8)
                    {
                        frameParsingError = new TransportException("specified frame data offset %d smaller than minimum frame header size %d", dataOffset, 8);
                        state = State.ERROR;
                        break;
                    }
                    else if(dataOffset > size)
                    {
                        frameParsingError = new TransportException("specified frame data offset %d larger than the frame size %d", dataOffset, _size);
                        state = State.ERROR;
                        break;
                    }

                    // type

                    int type = input.get() & 0xFF;
                    // SASL frame has no type-specific content in the frame header, so we skip next two bytes
                    input.get();
                    input.get();

                    if(type != SaslImpl.SASL_FRAME_TYPE)
                    {
                        frameParsingError = new TransportException("unknown frame type: %d", type);
                        state = State.ERROR;
                        break;
                    }

                    if(dataOffset!=8)
                    {
                        input.position(input.position()+dataOffset-8);
                    }

                    // oldIn null iff not working on duplicated buffer
                    if(oldIn == null)
                    {
                        oldIn = input;
                        input = input.duplicate();
                        final int endPos = input.position() + size - dataOffset;
                        input.limit(endPos);
                        oldIn.position(endPos);

                    }

                    try
                    {
                        _decoder.setByteBuffer(input);
                        Object val = _decoder.readObject();

                        Binary payload;

                        if(input.hasRemaining())
                        {
                            byte[] payloadBytes = new byte[input.remaining()];
                            input.get(payloadBytes);
                            payload = new Binary(payloadBytes);
                        }
                        else
                        {
                            payload = null;
                        }

                        if(val instanceof SaslFrameBody)
                        {
                            SaslFrameBody frameBody = (SaslFrameBody) val;
                            _sasl.handle(frameBody, payload);

                            reset();
                            input = oldIn;
                            oldIn = null;
                            _buffer = null;
                            state = State.SIZE_0;
                        }
                        else
                        {
                            state = State.ERROR;
                            frameParsingError = new TransportException("Unexpected frame type encountered."
                                                                       + " Found a %s which does not implement %s",
                                                                       val == null ? "null" : val.getClass(), SaslFrameBody.class);
                        }
                    }
                    catch (DecodeException ex)
                    {
                        state = State.ERROR;
                        frameParsingError = new TransportException(ex);
                    }
                    break;
                case ERROR:
                    // do nothing
            }

        }

        _state = state;
        _size = size;

        if(_state == State.ERROR)
        {
            if(frameParsingError != null)
            {
                throw frameParsingError;
            }
            else
            {
                throw new TransportException("Unable to parse, probably because of a previous error");
            }
        }
    }

    private void reset()
    {
        _size = 0;
        _state = State.SIZE_0;
    }
}
