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

public final class Decimal64 extends Number
{
    private final BigDecimal _underlying;
    private final long _bits;

    public Decimal64(BigDecimal underlying)
    {
        _underlying = underlying;
        _bits = calculateBits(underlying);

    }


    public Decimal64(final long bits)
    {
        _bits = bits;
        _underlying = calculateBigDecimal(bits);
    }

    static BigDecimal calculateBigDecimal(final long bits)
    {
        return BigDecimal.ZERO;
    }

    static long calculateBits(final BigDecimal underlying)
    {
        return 0l; // TODO
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

    public long getBits()
    {
        return _bits;
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

        final Decimal64 decimal64 = (Decimal64) o;

        if (_bits != decimal64._bits)
        {
            return false;
        }

        return true;
    }

    @Override
    public int hashCode()
    {
        return (int) (_bits ^ (_bits >>> 32));
    }
}
