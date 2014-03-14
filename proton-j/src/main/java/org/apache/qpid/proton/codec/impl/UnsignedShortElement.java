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

import org.apache.qpid.proton.amqp.UnsignedShort;
import org.apache.qpid.proton.codec.Data;

class UnsignedShortElement extends AtomicElement<UnsignedShort>
{

    private final UnsignedShort _value;

    UnsignedShortElement(Element parent, Element prev, UnsignedShort ub)
    {
        super(parent, prev);
        _value = ub;
    }

    @Override
    public int size()
    {
        return isElementOfArray() ? 2 : 3;
    }

    @Override
    public UnsignedShort getValue()
    {
        return _value;
    }

    @Override
    public Data.DataType getDataType()
    {
        return Data.DataType.USHORT;
    }

    @Override
    public int encode(ByteBuffer b)
    {
        if(isElementOfArray())
        {
            if(b.remaining()>=2)
            {
                b.putShort(_value.shortValue());
                return 2;
            }
        }
        else
        {
            if(b.remaining()>=3)
            {
                b.put((byte)0x60);
                b.putShort(_value.shortValue());
                return 3;
            }
        }
        return 0;
    }
}
