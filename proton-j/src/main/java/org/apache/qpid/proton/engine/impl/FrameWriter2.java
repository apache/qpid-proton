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

import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.nio.ByteBuffer;
import java.util.List;
import java.util.Map;

import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.DescribedType;
import org.apache.qpid.proton.amqp.transport.FrameBody;
import org.apache.qpid.proton.codec2.ByteArrayEncoder;
import org.apache.qpid.proton.codec2.CodecHelper;
import org.apache.qpid.proton.codec2.Type;
import org.apache.qpid.proton.framing.TransportFrame;
import org.apache.qpid.proton.framing.TransportFrame2;
import org.apache.qpid.proton.transport2.Performative;

/**
 * FrameWriter2
 * 
 * Copied FrameWrite.java and modified just enough to plug in the new codec.
 * 
 */

class FrameWriter2
{

    static final byte AMQP_FRAME_TYPE = 0;

    static final byte SASL_FRAME_TYPE = (byte) 1;

    static final int DEFUALT_BUFFER_SIZE = Integer.getInteger("proton.encoder.buffer_size", 2048);

    private ByteArrayEncoder _encoder;

    private byte[] _buffer = new byte[DEFUALT_BUFFER_SIZE];

    private int _maxFrameSize;

    private byte _frameType;

    final private Ref<ProtocolTracer> _protocolTracer;

    private TransportImpl2 _transport;

    private int _frameStart = 0;

    private int _payloadStart;

    private int _performativeSize;

    private int _position = 0;

    private int _read = 0;

    private long _framesOutput = 0;

    FrameWriter2(ByteArrayEncoder encoder, int maxFrameSize, byte frameType, Ref<ProtocolTracer> protocolTracer,
            TransportImpl2 transport)
    {
        _encoder = encoder;
        _encoder.init(_buffer, 0, _buffer.length);
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
        byte[] old = _buffer;
        _buffer = new byte[_buffer.length * 2];
        System.arraycopy(old, 0, _buffer, 0, _position);

        _encoder.init(_buffer, _position, _buffer.length * 2);
    }

    void writeHeader(byte[] header)
    {
        System.arraycopy(header, 0, _buffer, 0, header.length);
        _position = header.length;
    }

    private void startFrame()
    {
        _frameStart = _position;
    }

    private void writePerformative(Object frameBody)
    {
        while (_buffer.length - _position < 8)
        {
            grow();
        }

        while (true)
        {
            try
            {
                _encoder.setPosition(_frameStart + 8);
                CodecHelper.encodeObject(_encoder,frameBody);
                _position = _encoder.getPosition();
                break;
            }
            catch (IndexOutOfBoundsException e)
            {
                grow();
            }
        }

        _payloadStart = _position;
        _performativeSize = _payloadStart - _frameStart;
    }

    private void endFrame(int channel)
    {
        int frameSize = _position - _frameStart;
        ByteBuffer buf = ByteBuffer.wrap(_buffer, _frameStart, 8);
        buf.putInt(frameSize);
        buf.put((byte)2);
        buf.put(_frameType);
        buf.putShort((short) channel);
    }

    void writeFrame(int channel, Object frameBody, ByteBuffer payload, Runnable onPayloadTooLarge)
    {
        startFrame();

        writePerformative(frameBody);

        if (_maxFrameSize > 0 && payload != null && (payload.remaining() + _performativeSize) > _maxFrameSize)
        {
            if (onPayloadTooLarge != null)
            {
                onPayloadTooLarge.run();
            }
            writePerformative(frameBody);
        }

        ByteBuffer originalPayload = null;
        if (payload != null)
        {
            originalPayload = payload.duplicate();
        }

        // XXX: this is a bit of a hack but it eliminates duplicate
        // code, further refactor will fix this
        if (_frameType == AMQP_FRAME_TYPE)
        {
            TransportFrame2 frame = new TransportFrame2(channel, (Performative) frameBody, null);
            _transport.log(TransportImpl.OUTGOING, frame);

            /*ProtocolTracer tracer = _protocolTracer.get();
            if (tracer != null)
            {
                tracer.sentFrame(frame);
            }*/
        }

        int capacity;
        if (_maxFrameSize > 0)
        {
            capacity = _maxFrameSize - _performativeSize;
        }
        else
        {
            capacity = Integer.MAX_VALUE;
        }
        int payloadSize = Math.min(payload == null ? 0 : payload.remaining(), capacity);

        if (payloadSize > 0)
        {
            while (_buffer.length - _position < payloadSize)
            {
                grow();
            }

            payload.get(_buffer, _position, payloadSize);
            _position = _position + payloadSize;
        }
        endFrame(channel);
        _framesOutput += 1;
        /*try
        {
            FileOutputStream fout = new FileOutputStream("/home/rajith/data/" + ((Performative) frameBody).getClass().getSimpleName());
            fout.write(_buffer, _read, _position);
            fout.flush();
            fout.close();
        }
        catch (Exception e)
        {
            e.printStackTrace();
        }*/
        
    }

    void writeFrame(Object frameBody)
    {
        writeFrame(0, frameBody, null, null);
    }

    boolean isFull()
    {
        // XXX: this should probably be tunable
        return _position > 64 * 1024;
    }

    int readBytes(ByteBuffer dst)
    {
        int size = Math.min(_position - _read, dst.remaining());

        dst.put(_buffer, _read, size);
        _read = _read + size;

        if (_read == _position)
        {
            _position = 0;
            _read = 0;
        }

        return size;
    }

    long getFramesOutput()
    {
        return _framesOutput;
    }
}