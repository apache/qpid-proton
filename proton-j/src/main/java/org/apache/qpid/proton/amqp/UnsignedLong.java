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

import java.math.BigInteger;

public final class UnsignedLong extends Number implements Comparable<UnsignedLong>
{
    private static final BigInteger TWO_TO_THE_SIXTY_FOUR = new BigInteger( new byte[] { (byte) 1, (byte) 0, (byte) 0, (byte) 0, (byte) 0, (byte) 0, (byte) 0, (byte) 0, (byte) 0 });
    private static final BigInteger LONG_MAX_VALUE = BigInteger.valueOf(Long.MAX_VALUE);

    private static final UnsignedLong[] cachedValues = new UnsignedLong[256];

    static
    {
        for(int i = 0; i<256; i++)
        {
            cachedValues[i] = new UnsignedLong(i);
        }
    }

    public static final UnsignedLong ZERO = cachedValues[0];

    private final long _underlying;


    public UnsignedLong(long underlying)
    {
        _underlying = underlying;
    }

    @Override
    public int intValue()
    {
        return (int) _underlying;
    }

    @Override
    public long longValue()
    {
        return _underlying;
    }

    public BigInteger bigIntegerValue()
    {
        if(_underlying >= 0L)
        {
            return BigInteger.valueOf(_underlying);
        }
        else
        {
            return TWO_TO_THE_SIXTY_FOUR.add(BigInteger.valueOf(_underlying));
        }
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

        UnsignedLong that = (UnsignedLong) o;

        if (_underlying != that._underlying)
        {
            return false;
        }

        return true;
    }

    public int compareTo(UnsignedLong o)
    {
        return bigIntegerValue().compareTo(o.bigIntegerValue());
    }

    @Override
    public int hashCode()
    {
        return (int)(_underlying ^ (_underlying >>> 32));
    }

    @Override
    public String toString()
    {
        return String.valueOf(bigIntegerValue());
    }

    public static UnsignedLong valueOf(long underlying)
    {
        if((underlying & 0xFFL) == underlying)
        {
            return cachedValues[(int)underlying];
        }
        else
        {
            return new UnsignedLong(underlying);
        }
    }

    public static UnsignedLong valueOf(final String value)
    {
        BigInteger bigInt = new BigInteger(value);

        return valueOf(bigInt);
    }

    public static UnsignedLong valueOf(BigInteger bigInt)
    {
        if(bigInt.signum() == -1 || bigInt.bitLength() > 64)
        {
            throw new NumberFormatException("Value \""+bigInt+"\" lies outside the range [0 - 2^64).");
        }
        else if(bigInt.compareTo(LONG_MAX_VALUE)>=0)
        {
            return UnsignedLong.valueOf(bigInt.longValue());
        }
        else
        {
            return UnsignedLong.valueOf(TWO_TO_THE_SIXTY_FOUR.subtract(bigInt).negate().longValue());
        }
    }
}
