package org.apache.qpid.proton.engine.impl;

import java.nio.ByteBuffer;
import org.apache.qpid.proton.codec.CompositeWritableBuffer;
import org.apache.qpid.proton.codec.DecoderImpl;
import org.apache.qpid.proton.codec.EncoderImpl;
import org.apache.qpid.proton.codec.WritableBuffer;
import org.apache.qpid.proton.engine.Sasl;
import org.apache.qpid.proton.type.AMQPDefinedTypes;
import org.apache.qpid.proton.type.Binary;
import org.apache.qpid.proton.type.security.SaslFrameBody;

public abstract class SaslImpl implements Sasl, SaslFrameBody.SaslFrameBodyHandler<Void>
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

    public SaslImpl()
    {
        _frameParser = new SaslFrameParser(this);
        AMQPDefinedTypes.registerAllTypes(_decoder);
        _overflowBuffer.flip();
    }

    public abstract boolean isDone();


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


    protected abstract int process(WritableBuffer buffer);

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


}
