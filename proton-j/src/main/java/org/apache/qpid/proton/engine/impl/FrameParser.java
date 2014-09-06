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

import static org.apache.qpid.proton.engine.impl.AmqpHeader.HEADER;
import static org.apache.qpid.proton.engine.impl.ByteBufferUtils.newWriteableBuffer;

import java.nio.ByteBuffer;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.transport.FrameBody;
import org.apache.qpid.proton.codec.ByteBufferDecoder;
import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.engine.TransportException;
import org.apache.qpid.proton.framing.TransportFrame;

class FrameParser implements TransportInput
{
    private static final Logger TRACE_LOGGER = Logger.getLogger("proton.trace");

    private static final ByteBuffer _emptyInputBuffer = newWriteableBuffer(0);

    private enum State
    {
        HEADER0,
        HEADER1,
        HEADER2,
        HEADER3,
        HEADER4,
        HEADER5,
        HEADER6,
        HEADER7,
        SIZE_0,
        SIZE_1,
        SIZE_2,
        SIZE_3,
        PRE_PARSE,
        BUFFERING,
        PARSING,
        ERROR
    }

    private final FrameHandler _frameHandler;
    private final ByteBufferDecoder _decoder;
    private final int _maxFrameSize;

    private ByteBuffer _inputBuffer = null;
    private boolean _tail_closed = false;

    private State _state = State.HEADER0;

    /** the stated size of the current frame */
    private int _size;

    /** holds the current frame that is being parsed */
    private ByteBuffer _frameBuffer;

    private TransportFrame _heldFrame;
    private TransportException _parsingError;


    /**
     * We store the last result when processing input so that
     * we know not to process any more input if it was an error.
     */

    FrameParser(FrameHandler frameHandler, ByteBufferDecoder decoder, int maxFrameSize)
    {
        _frameHandler = frameHandler;
        _decoder = decoder;
        _maxFrameSize = maxFrameSize > 0 ? maxFrameSize : 4*1024;
    }

    private void input(ByteBuffer in) throws TransportException
    {
        flushHeldFrame();
        if (_heldFrame != null)
        {
            return;
        }

        TransportException frameParsingError = null;
        int size = _size;
        State state = _state;
        ByteBuffer oldIn = null;

        boolean transportAccepting = true;

        while(in.hasRemaining() && state != State.ERROR && transportAccepting)
        {
            switch(state)
            {
                case HEADER0:
                    if(in.hasRemaining())
                    {
                        byte c = in.get();
                        if(c != HEADER[0])
                        {
                            frameParsingError = new TransportException("AMQP header mismatch value %x, expecting %x. In state: %s", c, HEADER[0], state);
                            state = State.ERROR;
                            break;
                        }
                        state = State.HEADER1;
                    }
                    else
                    {
                        break;
                    }
                case HEADER1:
                    if(in.hasRemaining())
                    {
                        byte c = in.get();
                        if(c != HEADER[1])
                        {
                            frameParsingError = new TransportException("AMQP header mismatch value %x, expecting %x. In state: %s", c, HEADER[1], state);
                            state = State.ERROR;
                            break;
                        }
                        state = State.HEADER2;
                    }
                    else
                    {
                        break;
                    }
                case HEADER2:
                    if(in.hasRemaining())
                    {
                        byte c = in.get();
                        if(c != HEADER[2])
                        {
                            frameParsingError = new TransportException("AMQP header mismatch value %x, expecting %x. In state: %s", c, HEADER[2], state);
                            state = State.ERROR;
                            break;
                        }
                        state = State.HEADER3;
                    }
                    else
                    {
                        break;
                    }
                case HEADER3:
                    if(in.hasRemaining())
                    {
                        byte c = in.get();
                        if(c != HEADER[3])
                        {
                            frameParsingError = new TransportException("AMQP header mismatch value %x, expecting %x. In state: %s", c, HEADER[3], state);
                            state = State.ERROR;
                            break;
                        }
                        state = State.HEADER4;
                    }
                    else
                    {
                        break;
                    }
                case HEADER4:
                    if(in.hasRemaining())
                    {
                        byte c = in.get();
                        if(c != HEADER[4])
                        {
                            frameParsingError = new TransportException("AMQP header mismatch value %x, expecting %x. In state: %s", c, HEADER[4], state);
                            state = State.ERROR;
                            break;
                        }
                        state = State.HEADER5;
                    }
                    else
                    {
                        break;
                    }
                case HEADER5:
                    if(in.hasRemaining())
                    {
                        byte c = in.get();
                        if(c != HEADER[5])
                        {
                            frameParsingError = new TransportException("AMQP header mismatch value %x, expecting %x. In state: %s", c, HEADER[5], state);
                            state = State.ERROR;
                            break;
                        }
                        state = State.HEADER6;
                    }
                    else
                    {
                        break;
                    }
                case HEADER6:
                    if(in.hasRemaining())
                    {
                        byte c = in.get();
                        if(c != HEADER[6])
                        {
                            frameParsingError = new TransportException("AMQP header mismatch value %x, expecting %x. In state: %s", c, HEADER[6], state);
                            state = State.ERROR;
                            break;
                        }
                        state = State.HEADER7;
                    }
                    else
                    {
                        break;
                    }
                case HEADER7:
                    if(in.hasRemaining())
                    {
                        byte c = in.get();
                        if(c != HEADER[7])
                        {
                            frameParsingError = new TransportException("AMQP header mismatch value %x, expecting %x. In state: %s", c, HEADER[7], state);
                            state = State.ERROR;
                            break;
                        }
                        state = State.SIZE_0;
                    }
                    else
                    {
                        break;
                    }
                case SIZE_0:
                    if(!in.hasRemaining())
                    {
                        break;
                    }
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
                        frameParsingError = new TransportException("specified frame size %d smaller than minimum frame header "
                                                         + "size %d",
                                                         size, 8);
                        state = State.ERROR;
                        break;
                    }

                    if(in.remaining() < size-4)
                    {
                        _frameBuffer = ByteBuffer.allocate(size-4);
                        _frameBuffer.put(in);
                        state = State.BUFFERING;
                        break;
                    }
                case BUFFERING:
                    if(_frameBuffer != null)
                    {
                        if(in.remaining() < _frameBuffer.remaining())
                        {
                            _frameBuffer.put(in);
                            break;
                        }
                        else
                        {
                            ByteBuffer dup = in.duplicate();
                            dup.limit(dup.position()+_frameBuffer.remaining());
                            in.position(in.position()+_frameBuffer.remaining());
                            _frameBuffer.put(dup);
                            oldIn = in;
                            _frameBuffer.flip();
                            in = _frameBuffer;
                            state = State.PARSING;
                        }
                    }

                case PARSING:

                    int dataOffset = (in.get() << 2) & 0x3FF;

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

                    int type = in.get() & 0xFF;
                    int channel = in.getShort() & 0xFFFF;

                    if(type != 0)
                    {
                        frameParsingError = new TransportException("unknown frame type: %d", type);
                        state = State.ERROR;
                        break;
                    }

                    // note that this skips over the extended header if it's present
                    if(dataOffset!=8)
                    {
                        in.position(in.position()+dataOffset-8);
                    }

                    // oldIn null iff not working on duplicated buffer
                    final int frameBodySize = size - dataOffset;
                    if(oldIn == null)
                    {
                        oldIn = in;
                        in = in.duplicate();
                        final int endPos = in.position() + frameBodySize;
                        in.limit(endPos);
                        oldIn.position(endPos);

                    }

                    try
                    {
                        if (frameBodySize > 0)
                        {

                            _decoder.setByteBuffer(in);
                            Object val = _decoder.readObject();
                            _decoder.setByteBuffer(null);

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

                            if(val instanceof FrameBody)
                            {
                                FrameBody frameBody = (FrameBody) val;
                                if(TRACE_LOGGER.isLoggable(Level.FINE))
                                {
                                    TRACE_LOGGER.log(Level.FINE, "IN: CH["+channel+"] : " + frameBody + (payload == null ? "" : "[" + payload + "]"));
                                }
                                TransportFrame frame = new TransportFrame(channel, frameBody, payload);

                                if(_frameHandler.isHandlingFrames())
                                {
                                    _tail_closed = _frameHandler.handleFrame(frame);
                                }
                                else
                                {
                                    transportAccepting = false;
                                    _heldFrame = frame;
                                }

                            }
                            else
                            {
                                throw new TransportException("Frameparser encountered a "
                                        + (val == null? "null" : val.getClass())
                                        + " which is not a " + FrameBody.class);
                            }
                        }
                        else
                        {
                            if(TRACE_LOGGER.isLoggable(Level.FINEST))
                            {
                                TRACE_LOGGER.finest("Ignored empty frame");
                            }
                        }
                        reset();
                        in = oldIn;
                        oldIn = null;
                        _frameBuffer = null;
                        state = State.SIZE_0;
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

        if (_tail_closed)
        {
            if (in.hasRemaining()) {
                state = State.ERROR;
                frameParsingError = new TransportException("framing error");
            } else if (state != State.SIZE_0) {
                state = State.ERROR;
                frameParsingError = new TransportException("connection aborted");
            } else {
                _frameHandler.closed(null);
            }
        }

        _state = state;
        _size = size;

        if(_state == State.ERROR)
        {
            _tail_closed = true;
            if(frameParsingError != null)
            {
                _parsingError = frameParsingError;
                _frameHandler.closed(frameParsingError);
            }
            else
            {
                throw new TransportException("Unable to parse, probably because of a previous error");
            }
        }
    }

    @Override
    public int capacity()
    {
        if (_tail_closed) {
            return Transport.END_OF_STREAM;
        } else {
            if (_inputBuffer != null) {
                return _inputBuffer.remaining();
            } else {
                return _maxFrameSize;
            }
        }
    }

    @Override
    public ByteBuffer tail()
    {
        if (_tail_closed) {
            throw new TransportException("tail closed");
        }

        if (_inputBuffer == null) {
            _inputBuffer = newWriteableBuffer(_maxFrameSize);
        }

        return _inputBuffer;
    }

    @Override
    public void process() throws TransportException
    {
        if (_inputBuffer != null)
        {
            _inputBuffer.flip();

            try
            {
                input(_inputBuffer);
            }
            finally
            {
                if (_inputBuffer.hasRemaining()) {
                    _inputBuffer.compact();
                } else if (_inputBuffer.capacity() > TransportImpl.BUFFER_RELEASE_THRESHOLD) {
                    _inputBuffer = null;
                } else {
                    _inputBuffer.clear();
                }
            }
        }
        else
        {
            input(_emptyInputBuffer);
        }
    }

    @Override
    public void close_tail()
    {
        _tail_closed = true;
        process();
    }

    /**
     * Attempt to flush any cached data to the frame transport.  This function
     * is useful if the {@link FrameHandler} state has changed.
     */
    public void flush()
    {
        flushHeldFrame();

        if (_heldFrame == null)
        {
            process();
        }
    }

    private void flushHeldFrame()
    {
        if(_heldFrame != null && _frameHandler.isHandlingFrames())
        {
            _tail_closed = _frameHandler.handleFrame(_heldFrame);
            _heldFrame = null;
        }
    }

    private void reset()
    {
        _size = 0;
        _state = State.SIZE_0;
    }
}
