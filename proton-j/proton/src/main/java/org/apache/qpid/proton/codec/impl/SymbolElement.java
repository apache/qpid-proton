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

package org.apache.qpid.proton.codec.impl;

import java.nio.ByteBuffer;
import java.nio.charset.Charset;

import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.codec.Data;

class SymbolElement extends AtomicElement<Symbol>
{

    private static final Charset ASCII = Charset.forName("US-ASCII");
    private final Symbol _value;

    SymbolElement(Element parent, Element prev, Symbol s)
    {
        super(parent, prev);
        _value = s;
    }

    @Override
    public int size()
    {
        final int length = _value.length();

        if(isElementOfArray())
        {
            final ArrayElement parent = (ArrayElement) parent();

            if(parent.constructorType() == ArrayElement.SMALL)
            {
                if(length > 255)
                {
                    parent.setConstructorType(ArrayElement.LARGE);
                    return 4+length;
                }
                else
                {
                    return 1+length;
                }
            }
            else
            {
                return 4+length;
            }
        }
        else
        {
            if(length >255)
            {
                return 5 + length;
            }
            else
            {
                return 2 + length;
            }
        }
    }

    @Override
    public Symbol getValue()
    {
        return _value;
    }

    @Override
    public Data.DataType getDataType()
    {
        return Data.DataType.SYMBOL;
    }

    @Override
    public int encode(ByteBuffer b)
    {
        int size = size();
        if(b.remaining()<size)
        {
            return 0;
        }
        if(isElementOfArray())
        {
            final ArrayElement parent = (ArrayElement) parent();

            if(parent.constructorType() == ArrayElement.SMALL)
            {
                b.put((byte)_value.length());
            }
            else
            {
                b.putInt(_value.length());
            }
        }
        else if(_value.length()<=255)
        {
            b.put((byte)0xa3);
            b.put((byte)_value.length());
        }
        else
        {
            b.put((byte)0xb3);
            b.put((byte)_value.length());
        }
        b.put(_value.toString().getBytes(ASCII));
        return size;

    }
}
