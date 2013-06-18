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

import org.apache.qpid.proton.codec.Data;

class BooleanElement extends AtomicElement<Boolean>
{
    private final boolean _value;

    public BooleanElement(Element parent, Element current, boolean b)
    {
        super(parent, current);
        _value = b;
    }

    @Override
    public int size()
    {
        // in non-array parent then there is a single byte encoding, in an array there is a 1-byte encoding but no
        // constructor
        return 1;
    }

    @Override
    public Boolean getValue()
    {
        return _value;
    }

    @Override
    public Data.DataType getDataType()
    {
        return Data.DataType.BOOL;
    }

    @Override
    public int encode(ByteBuffer b)
    {
        if(b.hasRemaining())
        {
            if(isElementOfArray())
            {
                b.put(_value ? (byte) 1 : (byte) 0);
            }
            else
            {
                b.put(_value ? (byte) 0x41 : (byte) 0x42);
            }
            return 1;
        }
        return 0;
    }

}
