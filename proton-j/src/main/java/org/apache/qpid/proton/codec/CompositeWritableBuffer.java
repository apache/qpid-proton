package org.apache.qpid.proton.codec;

import java.nio.ByteBuffer;

public class CompositeWritableBuffer implements WritableBuffer
{
    private final WritableBuffer _first;
    private final WritableBuffer _second;

    public CompositeWritableBuffer(WritableBuffer first, WritableBuffer second)
    {
        _first = first;
        _second = second;
    }

    public void put(byte b)
    {
        (_first.hasRemaining() ? _first : _second).put(b);
    }

    public void putFloat(float f)
    {
        putInt(Float.floatToRawIntBits(f));
    }

    public void putDouble(double d)
    {
        putLong(Double.doubleToRawLongBits(d));
    }

    public void put(byte[] src, int offset, int length)
    {
        final int remaining = _first.remaining();
        if(length > remaining)
        {
            if(remaining == 0)
            {
                 _second.put(src,offset,length);
            }
            else
            {
                _first.put(src,offset, remaining);
                _second.put(src,offset+ remaining,length - remaining);
            }
        }
        else
        {
            _first.put(src,offset,length);
        }
    }

    public void putShort(short s)
    {
        switch(_first.remaining())
        {
            case 0:
                _second.putShort(s);
                break;
            case 1:
                _first.put((byte)(s >>> 8));
                _second.put((byte)s);
                break;
            default:
                _first.putShort(s);
        }
    }

    public void putInt(int i)
    {
        if(_first.remaining() >= 4)
        {
            _first.putInt(i);
        }
        else
        {
            int j = 3;
            while(_first.hasRemaining() && j != 0)
            {
                _first.put((byte)(i >>> (8*j)));
                j--;
            }
            while(j != 0)
            {
                _second.put((byte)(i >>> (8*j)));
            }
        }
    }

    public void putLong(long l)
    {
        if(_first.remaining() >= 8)
        {
            _first.putLong(l);
        }
        else
        {
            int j = 7;
            while(_first.hasRemaining() && j != 0)
            {
                _first.put((byte)(l >>> (8*j)));
                j--;
            }
            while(j != 0)
            {
                _second.put((byte)(l >>> (8*j)));
            }
        }
    }

    public boolean hasRemaining()
    {
        return _first.hasRemaining() || _second.hasRemaining();
    }

    public int remaining()
    {
        return _first.remaining()+_second.remaining();
    }

    public int position()
    {
        return _first.position()+_second.position();
    }

    public void position(int position)
    {
        int currentPosition = position();
        if(position > currentPosition)
        {
            int relativePosition = position - currentPosition;
            if(_first.hasRemaining())
            {
                final int firstRemaining = _first.remaining();
                if(relativePosition > firstRemaining)
                {
                    _first.position(_first.position()+ firstRemaining);
                    relativePosition -= firstRemaining;
                }
                else
                {
                    _first.position(_first.position()+relativePosition);
                    return;
                }
            }
            _second.position(_second.position()+relativePosition);
        }
        else if(position < currentPosition)
        {
            if(_first.hasRemaining())
            {
                _first.position(position);
            }
            else
            {
                int relativePosition = currentPosition-position;
                if(relativePosition <= _second.position())
                {
                    _second.position(_second.position()-relativePosition);
                }
                else
                {
                    relativePosition -= _second.position();
                    _second.position(0);
                    _first.position(_first.position()-relativePosition);
                }
            }
        }
    }

    public void put(ByteBuffer payload)
    {
        if(_first.hasRemaining())
        {
            if(_first.remaining() >= payload.remaining())
            {
                _first.put(payload);
            }
            else
            {
                int limit = payload.limit();
                payload.limit(payload.position()+_first.remaining());
                _first.put(payload);
                payload.limit(limit);
                _second.put(payload);
            }
        }
        else
        {
            _second.put(payload);
        }
    }
}
