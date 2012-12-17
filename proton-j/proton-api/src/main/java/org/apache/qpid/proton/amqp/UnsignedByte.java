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

public final class UnsignedByte extends Number implements Comparable<UnsignedByte>
{
    private final byte _underlying;
    private static final UnsignedByte[] cachedValues = new UnsignedByte[256];

    static
    {
        for(int i = 0; i<256; i++)
        {
            cachedValues[i] = new UnsignedByte((byte)i);
        }
    }

    public UnsignedByte(byte underlying)
    {
        _underlying = underlying;
    }

    @Override
    public byte byteValue()
    {
        return _underlying;
    }

    @Override
    public short shortValue()
    {
        return (short) intValue();
    }

    @Override
    public int intValue()
    {
        return ((int)_underlying) & 0xFF;
    }

    @Override
    public long longValue()
    {
        return ((long) _underlying) & 0xFFl;
    }

    @Override
    public float floatValue()
    {
        return (float) longValue();
    }

    @Override
    public double doubleValue()
    {
        return (double) longValue();
    }

    @Override
    public boolean equals(Object o)
    {
        if (this == o)
        {
            return true;
        }
        if (o == null || getClass() != o.getClass())
        {
            return false;
        }

        UnsignedByte that = (UnsignedByte) o;

        if (_underlying != that._underlying)
        {
            return false;
        }

        return true;
    }

    public int compareTo(UnsignedByte o)
    {
        return Integer.signum(intValue() - o.intValue());
    }

    @Override
    public int hashCode()
    {
        return _underlying;
    }

    @Override
    public String toString()
    {
        return String.valueOf(intValue());
    }

    public static UnsignedByte valueOf(byte underlying)
    {
        final int index = ((int) underlying) & 0xFF;
        return cachedValues[index];
    }

    public static UnsignedByte valueOf(final String value)
            throws NumberFormatException
    {
        int intVal = Integer.parseInt(value);
        if(intVal < 0 || intVal >= (1<<8))
        {
            throw new NumberFormatException("Value \""+value+"\" lies outside the range [" + 0 + "-" + (1<<8) +").");
        }
        return valueOf((byte)intVal);
    }

}
