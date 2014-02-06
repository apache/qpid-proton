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
import java.util.*;

import org.apache.qpid.proton.codec.Data;

class MapElement extends AbstractElement<Map<Object,Object>>
{
    private Element _first;

    MapElement(Element parent, Element prev)
    {
        super(parent, prev);
    }


    public int count()
    {
        int count = 0;
        Element elt = _first;
        while(elt != null)
        {
            count++;
            elt = elt.next();
        }
        return count;
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
            ArrayElement parent = (ArrayElement) parent();

            if(parent.constructorType() == ArrayElement.SMALL)
            {
                if(count > 255 || size > 254)
                {
                    parent.setConstructorType(ArrayElement.ConstructorType.LARGE);
                    size += 8;
                }
                else
                {
                    size += 2;
                }
            }
            size += 8;

        }
        else
        {
            if(count <= 255 && size <= 254)
            {
                size += 3;
            }
            else
            {
                size+=9;
            }
        }

        return size;
    }

    @Override
    public Map<Object,Object> getValue()
    {
        LinkedHashMap<Object,Object> map = new LinkedHashMap<Object,Object>();
        Element elt = _first;
        while(elt != null)
        {
            Object key = elt.getValue();
            Object value;
            elt = elt.next();
            if(elt != null)
            {
                value = elt.getValue();
                elt = elt.next();
            }
            else
            {
                value = null;
            }
            map.put(key,value);
        }

        return Collections.unmodifiableMap(map);
    }

    @Override
    public Data.DataType getDataType()
    {
        return Data.DataType.MAP;
    }

    @Override
    public int encode(ByteBuffer b)
    {
        int encodedSize = size();

        int count = 0;
        int size = 0;
        Element elt = _first;
        while(elt != null)
        {
            count++;
            size += elt.size();
            elt = elt.next();
        }

        if(encodedSize > b.remaining())
        {
            return 0;
        }
        else
        {
            if(isElementOfArray())
            {
                switch(((ArrayElement)parent()).constructorType())
                {
                    case SMALL:
                        b.put((byte)(size+1));
                        b.put((byte)count);
                        break;
                    case LARGE:
                        b.putInt((size+4));
                        b.putInt(count);
                }
            }
            else
            {
                if(size <= 254 && count <=255)
                {
                    b.put((byte)0xc1);
                    b.put((byte)(size+1));
                    b.put((byte)count);
                }
                else
                {
                    b.put((byte)0xd1);
                    b.putInt((size+4));
                    b.putInt(count);
                }

            }

            elt = _first;
            while(elt != null)
            {
                elt.encode(b);
                elt = elt.next();
            }

            return encodedSize;
        }
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
        return "{";
    }

    @Override
    String stopSymbol() {
        return "}";
    }

}
