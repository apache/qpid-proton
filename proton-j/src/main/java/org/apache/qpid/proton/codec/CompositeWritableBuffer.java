package org.apache.qpid.proton.codec;
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

    public void putShort(short s)
    {
        int remaining = _first.remaining();
        if(remaining >= 2)
        {
            _first.putShort(s);
        }
        else if(remaining ==0 )
        {
            _second.putShort(s);
        }
        else
        {
            ByteBuffer wrap = ByteBuffer.wrap(new byte[2]);
            wrap.putShort(s);
            wrap.flip();
            put(wrap);
        }
    }

    public void putInt(int i)
    {
        int remaining = _first.remaining();
        if(remaining >= 4)
        {
            _first.putInt(i);
        }
        else if(remaining ==0 )
        {
            _second.putInt(i);
        }
        else
        {
            ByteBuffer wrap = ByteBuffer.wrap(new byte[4]);
            wrap.putInt(i);
            wrap.flip();
            put(wrap);
        }
    }

    public void putLong(long l)
    {
        int remaining = _first.remaining();
        if(remaining >= 8)
        {
            _first.putLong(l);
        }
        else if(remaining ==0 )
        {
            _second.putLong(l);
        }
        else
        {
            ByteBuffer wrap = ByteBuffer.wrap(new byte[8]);
            wrap.putLong(l);
            wrap.flip();
            put(wrap);
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

    public int limit()
    {
        return _first.limit() + _second.limit();
    }

    public void position(int position)
    {
        int first_limit = _first.limit();
        if( position <= first_limit )
        {
            _first.position(position);
            _second.position(0);
        }
        else
        {
            _first.position(first_limit);
            _second.position(position - first_limit);
        }
    }

    public void put(byte[] src, int offset, int length)
    {
        final int firstRemaining = _first.remaining();
        if(firstRemaining > 0)
        {
            if(firstRemaining >= length)
            {
                _first.put(src, offset, length);
                return;
            }
            else
            {
                _first.put(src,offset, firstRemaining);
            }
        }
        _second.put(src, offset+firstRemaining, length-firstRemaining);
    }

    public void put(ByteBuffer payload)
    {
        int firstRemaining = _first.remaining();
        if(firstRemaining > 0)
        {
            if(firstRemaining >= payload.remaining())
            {
                _first.put(payload);
                return;
            }
            else
            {
                int limit = payload.limit();
                payload.limit(payload.position()+firstRemaining);
                _first.put(payload);
                payload.limit(limit);
            }
        }
        _second.put(payload);
    }

    @Override
    public String toString()
    {
        return _first.toString() + " + "+_second.toString();
    }
}
