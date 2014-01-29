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

import java.math.BigDecimal;
import java.nio.ByteBuffer;

public final class Decimal128 extends Number
{
    private final BigDecimal _underlying;
    private final long _msb;
    private final long _lsb;

    public Decimal128(BigDecimal underlying)
    {
        _underlying = underlying;

        _msb = calculateMostSignificantBits(underlying);
        _lsb = calculateLeastSignificantBits(underlying);
    }


    public Decimal128(final long msb, final long lsb)
    {
        _msb = msb;
        _lsb = lsb;

        _underlying = calculateBigDecimal(msb, lsb);

    }

    public Decimal128(byte[] data)
    {
        this(ByteBuffer.wrap(data));
    }

    private Decimal128(final ByteBuffer buffer)
    {
        this(buffer.getLong(),buffer.getLong());
    }

    private static long calculateMostSignificantBits(final BigDecimal underlying)
    {
        return 0;  //TODO.
    }

    private static long calculateLeastSignificantBits(final BigDecimal underlying)
    {
        return 0;  //TODO.
    }

    private static BigDecimal calculateBigDecimal(final long msb, final long lsb)
    {
        return BigDecimal.ZERO;  //TODO.
    }

    @Override
    public int intValue()
    {
        return _underlying.intValue();
    }

    @Override
    public long longValue()
    {
        return _underlying.longValue();
    }

    @Override
    public float floatValue()
    {
        return _underlying.floatValue();
    }

    @Override
    public double doubleValue()
    {
        return _underlying.doubleValue();
    }

    public long getMostSignificantBits()
    {
        return _msb;
    }

    public long getLeastSignificantBits()
    {
        return _lsb;
    }

    public byte[] asBytes()
    {
        byte[] bytes = new byte[16];
        ByteBuffer buf = ByteBuffer.wrap(bytes);
        buf.putLong(getMostSignificantBits());
        buf.putLong(getLeastSignificantBits());
        return bytes;
    }

    @Override
    public boolean equals(final Object o)
    {
        if (this == o)
        {
            return true;
        }
        if (o == null || getClass() != o.getClass())
        {
            return false;
        }

        final Decimal128 that = (Decimal128) o;

        if (_lsb != that._lsb)
        {
            return false;
        }
        if (_msb != that._msb)
        {
            return false;
        }

        return true;
    }

    @Override
    public int hashCode()
    {
        int result = (int) (_msb ^ (_msb >>> 32));
        result = 31 * result + (int) (_lsb ^ (_lsb >>> 32));
        return result;
    }
}
