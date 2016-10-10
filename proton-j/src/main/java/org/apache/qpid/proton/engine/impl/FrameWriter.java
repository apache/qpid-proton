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

import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.transport.EmptyFrame;
import org.apache.qpid.proton.amqp.transport.FrameBody;
import org.apache.qpid.proton.codec.EncoderImpl;
import org.apache.qpid.proton.codec.WritableBuffer;
import org.apache.qpid.proton.framing.TransportFrame;

import java.nio.BufferOverflowException;
import java.nio.ByteBuffer;

/**
 * FrameWriter
 *
 */

class FrameWriter
{

    static final byte AMQP_FRAME_TYPE = 0;
    static final byte SASL_FRAME_TYPE = (byte) 1;

    private EncoderImpl _encoder;
    private ByteBuffer _bbuf;
    private WritableBuffer _buffer;
    private int _maxFrameSize;
    private byte _frameType;
    final private Ref<ProtocolTracer> _protocolTracer;
    private TransportImpl _transport;

    private int _frameStart = 0;
    private int _payloadStart;
    private int _performativeSize;
    private long _framesOutput = 0;

    FrameWriter(EncoderImpl encoder, int maxFrameSize, byte frameType,
                Ref<ProtocolTracer> protocolTracer, TransportImpl transport)
    {
        _encoder = encoder;
        _bbuf = ByteBuffer.allocate(1024);
        _buffer = new WritableBuffer.ByteBufferWrapper(_bbuf);
        _maxFrameSize = maxFrameSize;
        _frameType = frameType;
        _protocolTracer = protocolTracer;
        _transport = transport;
    }

    void setMaxFrameSize(int maxFrameSize)
    {
        _maxFrameSize = maxFrameSize;
    }

    private void grow()
    {
        ByteBuffer old = _bbuf;
        _bbuf = ByteBuffer.allocate(_bbuf.capacity() * 2);
        _buffer = new WritableBuffer.ByteBufferWrapper(_bbuf);
        old.flip();
        _bbuf.put(old);
    }

    void writeHeader(byte[] header)
    {
        _buffer.put(header, 0, header.length);
    }

    private void startFrame()
    {
        _frameStart = _buffer.position();
    }

    private void writePerformative(Object frameBody)
    {
        while (_buffer.remaining() < 8) {
            grow();
        }

        while (true)
        {
            try
            {
                _buffer.position(_frameStart + 8);
                if (frameBody != null) _encoder.writeObject(_buffer, frameBody);
                break;
            }
            catch (BufferOverflowException e)
            {
                grow();
            }
        }

        _payloadStart = _buffer.position();
        _performativeSize = _payloadStart - _frameStart;
    }

    private void endFrame(int channel)
    {
        int frameSize = _buffer.position() - _frameStart;
        int limit = _buffer.position();
        _buffer.position(_frameStart);
        _buffer.putInt(frameSize);
        _buffer.put((byte) 2);
        _buffer.put(_frameType);
        _buffer.putShort((short) channel);
        _buffer.position(limit);
    }

    void writeFrame(int channel, Object frameBody, ByteBuffer payload,
                    Runnable onPayloadTooLarge)
    {
        startFrame();

        writePerformative(frameBody);

        if(_maxFrameSize > 0 && payload != null && (payload.remaining() + _performativeSize) > _maxFrameSize)
        {
            if(onPayloadTooLarge != null)
            {
                onPayloadTooLarge.run();
            }
            writePerformative(frameBody);
        }

        int capacity;
        if (_maxFrameSize > 0) {
            capacity = _maxFrameSize - _performativeSize;
        } else {
            capacity = Integer.MAX_VALUE;
        }
        int payloadSize = Math.min(payload == null ? 0 : payload.remaining(), capacity);

        ProtocolTracer tracer = _protocolTracer == null ? null : _protocolTracer.get();
        if( tracer != null || _transport.isTraceFramesEnabled())
        {
            // XXX: this is a bit of a hack but it eliminates duplicate
            // code, further refactor will fix this
            if (_frameType == AMQP_FRAME_TYPE)
            {
                ByteBuffer originalPayload = null;
                if( payload!=null )
                {
                    originalPayload = payload.duplicate();
                    originalPayload.limit(payload.position() + payloadSize);
                }

                Binary payloadBin = Binary.create(originalPayload);
                FrameBody body = null;
                if (frameBody == null)
                {
                    body = new EmptyFrame();
                }
                else
                {
                    body = (FrameBody) frameBody;
                }

                TransportFrame frame = new TransportFrame(channel, body, payloadBin);

                _transport.log(TransportImpl.OUTGOING, frame);

                if(tracer != null)
                {
                    tracer.sentFrame(frame);
                }
            }
        }

        if(payloadSize > 0)
        {
            while (_buffer.remaining() < payloadSize) {
                grow();
            }

            int oldLimit = payload.limit();
            payload.limit(payload.position() + payloadSize);
            _buffer.put(payload);
            payload.limit(oldLimit);
        }

        endFrame(channel);

        _framesOutput += 1;
    }

    void writeFrame(Object frameBody)
    {
        writeFrame(0, frameBody, null, null);
    }

    boolean isFull() {
        // XXX: this should probably be tunable
        return _bbuf.position() > 64*1024;
    }

    int readBytes(ByteBuffer dst)
    {
        ByteBuffer src = _bbuf.duplicate();
        src.flip();

        int size = Math.min(src.remaining(), dst.remaining());
        int limit = src.limit();
        src.limit(size);
        dst.put(src);
        src.limit(limit);
        _bbuf.rewind();
        _bbuf.put(src);

        return size;
    }

    long getFramesOutput()
    {
        return _framesOutput;
    }
}
