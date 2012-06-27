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
import java.util.Formatter;
import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.engine.EndpointError;
import org.apache.qpid.proton.type.Binary;
import org.apache.qpid.proton.type.security.SaslFrameBody;

class SaslFrameParser
{

    private EndpointError _localError;
    private SaslImpl _sasl;



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


    SaslFrameParser(SaslImpl sasl)
    {
        _sasl = sasl;
    }

    public int input(byte[] bytes, int offset, final int length)
    {
        int unconsumed = length;
        EndpointError frameParsingError = null;
        int size = _size;
        State state = _state;
        ByteBuffer oldIn = null;

        // TODO protocol header hack
        if(_ignore != 0)
        {
            if(unconsumed > _ignore)
            {
                offset+=_ignore;
                unconsumed -= _ignore;

                _ignore = 0;
            }
            else
            {
                _ignore-=length;
                return length;
            }
        }

        ByteBuffer in = ByteBuffer.wrap(bytes, offset, unconsumed);

        while(in.hasRemaining() && state != State.ERROR && !_sasl.isDone())
        {
            switch(state)
            {
                case SIZE_0:
                    if(in.remaining() >= 4)
                    {
                        size = in.getInt();
                        state = State.PRE_PARSE;
                        break;
                    }
                    else
                    {
                        size = (in.get() << 24) & 0xFF000000;
                        if(!in.hasRemaining())
                        {
                            state = State.SIZE_1;
                            break;
                        }
                    }
                case SIZE_1:
                    size |= (in.get() << 16) & 0xFF0000;
                    if(!in.hasRemaining())
                    {
                        state = State.SIZE_2;
                        break;
                    }
                case SIZE_2:
                    size |= (in.get() << 8) & 0xFF00;
                    if(!in.hasRemaining())
                    {
                        state = State.SIZE_3;
                        break;
                    }
                case SIZE_3:
                    size |= in.get() & 0xFF;
                    state = State.PRE_PARSE;

                case PRE_PARSE:
                    ;
                    if(size < 8)
                    {
                        frameParsingError = createFramingError("specified frame size %d smaller than minimum frame header "
                                                         + "size %d",
                                                         _size, 8);
                        state = State.ERROR;
                        break;
                    }

                    if(in.remaining() < size-4)
                    {
                        _buffer = ByteBuffer.allocate(size-4);
                        _buffer.put(in);
                        state = State.BUFFERING;
                        break;
                    }
                case BUFFERING:
                    if(_buffer != null)
                    {
                        if(in.remaining() < _buffer.remaining())
                        {
                            _buffer.put(in);
                            break;
                        }
                        else
                        {
                            ByteBuffer dup = in.duplicate();
                            dup.limit(dup.position()+_buffer.remaining());
                            int i = _buffer.remaining();
                            int d = dup.remaining();
                            in.position(in.position()+_buffer.remaining());
                            _buffer.put(dup);
                            oldIn = in;
                            _buffer.flip();
                            in = _buffer;
                            state = State.PARSING;
                        }
                    }

                case PARSING:

                    int dataOffset = (in.get() << 2) & 0x3FF;

                    if(dataOffset < 8)
                    {
                        frameParsingError = createFramingError("specified frame data offset %d smaller than minimum frame header size %d", dataOffset, 8);
                        state = State.ERROR;
                        break;
                    }
                    else if(dataOffset > size)
                    {
                        frameParsingError = createFramingError("specified frame data offset %d larger than the frame size %d", dataOffset, _size);
                        state = State.ERROR;
                        break;
                    }

                    // type

                    int type = in.get() & 0xFF;
                    int channel = in.getShort() & 0xFF;

                    if(type != 1)
                    {
                        frameParsingError = createFramingError("unknown frame type: %d", type);
                        state = State.ERROR;
                        break;
                    }

                    if(dataOffset!=8)
                    {
                        in.position(in.position()+dataOffset-8);
                    }

                    // oldIn null iff not working on duplicated buffer
                    if(oldIn == null)
                    {
                        oldIn = in;
                        in = in.duplicate();
                        final int endPos = in.position() + size - dataOffset;
                        in.limit(endPos);
                        oldIn.position(endPos);

                    }

                    try
                    {
                        _sasl.getDecoder().setByteBuffer(in);
                        Object val = _sasl.getDecoder().readObject();

                        Binary payload;

                        if(in.hasRemaining())
                        {
                            byte[] payloadBytes = new byte[in.remaining()];
                            in.get(payloadBytes);
                            payload = new Binary(payloadBytes);
                        }
                        else
                        {
                            payload = null;
                        }

                        if(val instanceof SaslFrameBody)
                        {
                            SaslFrameBody frameBody = (SaslFrameBody) val;
                            frameBody.invoke(_sasl, payload,null);

                        }
                        else
                        {
                            // TODO - error
                        }
                        reset();
                        in = oldIn;
                        oldIn = null;
                        _buffer = null;
                        state = State.SIZE_0;
                        break;


                    }
                    catch (DecodeException ex)
                    {
                        state = State.ERROR;
                        frameParsingError = createFramingError(ex.getMessage());
                    }
            }

        }

        _state = state;
        _size = size;

        _localError = frameParsingError;

        return _state == State.ERROR ? -1 : length - in.remaining();
    }

    private void reset()
    {
        _size = 0;
        _state = State.SIZE_0;
    }


    private EndpointError createFramingError(String description, Object... args)
    {
        Formatter formatter = new Formatter();
        formatter.format(description, args);
//        System.out.println(formatter.toString());
        return null;  //TODO.
    }

}
