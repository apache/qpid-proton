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

import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.codec.Data;

class UnsignedIntegerElement extends AtomicElement<UnsignedInteger>
{

    private final UnsignedInteger _value;

    UnsignedIntegerElement(Element parent, Element prev, UnsignedInteger i)
    {
        super(parent, prev);
        _value = i;
    }

    @Override
    public int size()
    {
        if(isElementOfArray())
        {
            final ArrayElement parent = (ArrayElement) parent();
            if(parent.constructorType() == ArrayElement.TINY)
            {
                if(_value.intValue() == 0)
                {
                    return 0;
                }
                else
                {
                    parent.setConstructorType(ArrayElement.SMALL);
                }
            }

            if(parent.constructorType() == ArrayElement.SMALL)
            {
                if(0 <= _value.intValue() && _value.intValue() <= 255)
                {
                    return 1;
                }
                else
                {
                    parent.setConstructorType(ArrayElement.LARGE);
                }
            }

            return 4;

        }
        else
        {
            return 0 == _value.intValue() ? 1 : (1 <= _value.intValue() && _value.intValue() <= 255) ? 2 : 5;
        }

    }

    @Override
    public UnsignedInteger getValue()
    {
        return _value;
    }

    @Override
    public Data.DataType getDataType()
    {
        return Data.DataType.UINT;
    }

    @Override
    public int encode(ByteBuffer b)
    {
        int size = size();
        if(size > b.remaining())
        {
            return 0;
        }
        switch(size)
        {
            case 1:
                if(isElementOfArray())
                {
                    b.put((byte)_value.intValue());
                }
                else
                {
                    b.put((byte)0x43);
                }
                break;
            case 2:
                b.put((byte)0x52);
                b.put((byte)_value.intValue());
                break;
            case 5:
                b.put((byte)0x70);
            case 4:
                b.putInt(_value.intValue());

        }

        return size;
    }
}
