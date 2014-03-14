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
package org.apache.qpid.proton.amqp;

import java.nio.ByteBuffer;
import java.util.Collection;

import static java.lang.Math.min;

public final class Binary
{

    private final byte[] _data;
    private final int _offset;
    private final int _length;
    private int _hashCode;

    public Binary(final byte[] data)
    {
        this(data, 0, data.length);
    }

    public Binary(final byte[] data, final int offset, final int length)
    {
        _data = data;
        _offset = offset;
        _length = length;
    }

    public ByteBuffer asByteBuffer()
    {
        return ByteBuffer.wrap(_data, _offset, _length);
    }

    public final int hashCode()
    {
        int hc = _hashCode;
        if(hc == 0)
        {
            for (int i = 0; i < _length; i++)
            {
                hc = 31*hc + (0xFF & _data[_offset + i]);
            }
            _hashCode = hc;
        }
        return hc;
    }

    public final boolean equals(Object o)
    {
        Binary buf = (Binary) o;
        if(o == null)
        {
            return false;
        }
        final int size = _length;
        if (size != buf._length)
        {
            return false;
        }

        final byte[] myData = _data;
        final byte[] theirData = buf._data;
        int myOffset = _offset;
        int theirOffset = buf._offset;
        final int myLimit = myOffset + size;

        while(myOffset < myLimit)
        {
            if (myData[myOffset++] != theirData[theirOffset++])
            {
                return false;
            }
        }

        return true;
    }


    public int getArrayOffset()
    {
        return _offset;
    }

    public byte[] getArray()
    {
        return _data;
    }

    public int getLength()
    {
        return _length;
    }

    public String toString()
    {
        StringBuilder str = new StringBuilder();


        for (int i = 0; i < _length; i++)
        {
            byte c = _data[_offset + i];

            if (c > 31 && c < 127 && c != '\\')
            {
                str.append((char)c);
            }
            else
            {
                str.append(String.format("\\x%02x", c));
            }
        }

        return str.toString();

    }

    public static Binary combine(final Collection<Binary> binaries)
    {

        if(binaries.size() == 1)
        {
            return binaries.iterator().next();
        }

        int size = 0;
        for(Binary binary : binaries)
        {
            size += binary.getLength();
        }
        byte[] data = new byte[size];
        int offset = 0;
        for(Binary binary : binaries)
        {
            System.arraycopy(binary._data, binary._offset, data, offset, binary._length);
            offset += binary._length;
        }
        return new Binary(data);
    }

    public Binary subBinary(final int offset, final int length)
    {
        return new Binary(_data, _offset+offset, length);
    }

    public static Binary create(ByteBuffer buffer) 
    {
        if( buffer == null )
            return null;
        if( buffer.isDirect() || buffer.isReadOnly() )
        {
            byte data[] = new byte [buffer.remaining()];
            ByteBuffer dup = buffer.duplicate();
            dup.get(data);
            return new Binary(data);
        }
        else 
        {
            return new Binary(buffer.array(), buffer.arrayOffset()+buffer.position(), buffer.remaining());
        }
    }

}
