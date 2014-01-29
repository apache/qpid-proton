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

import java.util.concurrent.ConcurrentHashMap;

public final class Symbol implements Comparable<Symbol>, CharSequence
{
    private final String _underlying;
    private static final ConcurrentHashMap<String, Symbol> _symbols = new ConcurrentHashMap<String, Symbol>(2048);

    private Symbol(String underlying)
    {
        _underlying = underlying;
    }

    public int length()
    {
        return _underlying.length();
    }

    public int compareTo(Symbol o)
    {
        return _underlying.compareTo(o._underlying);
    }

    public char charAt(int index)
    {
        return _underlying.charAt(index);
    }

    public CharSequence subSequence(int beginIndex, int endIndex)
    {
        return _underlying.subSequence(beginIndex, endIndex);
    }

    @Override
    public String toString()
    {
        return _underlying;
    }

    @Override
    public int hashCode()
    {
        return _underlying.hashCode();
    }

    public static Symbol valueOf(String symbolVal)
    {
        return getSymbol(symbolVal);
    }

    public static Symbol getSymbol(String symbolVal)
    {
        if(symbolVal == null)
        {
            return null;
        }
        Symbol symbol = _symbols.get(symbolVal);
        if(symbol == null)
        {
            symbolVal = symbolVal.intern();
            symbol = new Symbol(symbolVal);
            Symbol existing;
            if((existing = _symbols.putIfAbsent(symbolVal, symbol)) != null)
            {
                symbol = existing;
            }
        }
        return symbol;
    }


}
