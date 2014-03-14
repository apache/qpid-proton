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
import java.util.AbstractSequentialList;
import java.util.ArrayList;
import java.util.List;
import java.util.ListIterator;

import org.apache.qpid.proton.amqp.DescribedType;
import org.apache.qpid.proton.codec.Data;

class DescribedTypeElement extends AbstractElement<DescribedType>
{
    private Element _first;

    DescribedTypeElement(Element parent, Element prev)
    {
        super(parent, prev);
    }




    @Override
    public int size()
    {
        int count = 0;
        int size = 0;
        Element elt = _first;
        while(elt != null)
        {
            count++;
            size += elt.size();
            elt = elt.next();
        }

        if(isElementOfArray())
        {
            throw new IllegalArgumentException("Cannot add described type members to an array");
        }
        else if(count > 2)
        {
            throw new IllegalArgumentException("Too many elements in described type");
        }
        else if(count == 0)
        {
            size = 3;
        }
        else if(count == 1)
        {
            size += 2;
        }
        else
        {
            size+=1;
        }

        return size;
    }

    @Override
    public DescribedType getValue()
    {
        final Object descriptor = _first == null ? null : _first.getValue();
        Element second = _first == null ? null : _first.next();
        final Object described = second == null ? null : second.getValue();
        return new DescribedTypeImpl(descriptor,described);
    }

    @Override
    public Data.DataType getDataType()
    {
        return Data.DataType.DESCRIBED;
    }

    @Override
    public int encode(ByteBuffer b)
    {
        int encodedSize = size();

        if(encodedSize > b.remaining())
        {
            return 0;
        }
        else
        {
            b.put((byte) 0);
            if(_first == null)
            {
                b.put((byte)0x40);
                b.put((byte)0x40);
            }
            else
            {
                _first.encode(b);
                if(_first.next() == null)
                {
                    b.put((byte)0x40);
                }
                else
                {
                    _first.next().encode(b);
                }
            }
        }
        return encodedSize;
    }

    @Override
    public boolean canEnter()
    {
        return true;
    }

    @Override
    public Element child()
    {
        return _first;
    }

    @Override
    public void setChild(Element elt)
    {
        _first = elt;
    }

    @Override
    public Element checkChild(Element element)
    {
        if(element.prev() != _first)
        {
            throw new IllegalArgumentException("Described Type may only have two elements");
        }
        return element;

    }

    @Override
    public Element addChild(Element element)
    {
        _first = element;
        return element;
    }

    @Override
    String startSymbol() {
        return "(";
    }

    @Override
    String stopSymbol() {
        return ")";
    }

}
