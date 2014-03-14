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

import org.apache.qpid.proton.amqp.DescribedType;
import org.apache.qpid.proton.codec.Data;

class ArrayElement extends AbstractElement<Object[]>
{

    private final boolean _described;
    private final Data.DataType _arrayType;
    private ConstructorType _constructorType;
    private Element _first;


    static enum ConstructorType { TINY, SMALL, LARGE }


    static ConstructorType TINY = ConstructorType.TINY;
    static ConstructorType SMALL = ConstructorType.SMALL;
    static ConstructorType LARGE = ConstructorType.LARGE;

    ArrayElement(Element parent, Element prev, boolean described, Data.DataType type)
    {
        super(parent, prev);
        _described = described;
        _arrayType = type;
        if(_arrayType == null)
        {
            throw new NullPointerException("Array type cannot be null");
        }
        else if(_arrayType == Data.DataType.DESCRIBED)
        {
            throw new IllegalArgumentException("Array type cannot be DESCRIBED");
        }
        switch(_arrayType)
        {
            case UINT:
            case ULONG:
            case LIST:
                setConstructorType(TINY);
                break;
            default:
                setConstructorType(SMALL);
        }
    }

    ConstructorType constructorType()
    {
        return _constructorType;
    }

    void setConstructorType(ConstructorType type)
    {
        _constructorType = type;
    }

    @Override
    public int size()
    {
        ConstructorType oldConstructorType;
        int bodySize;
        int count = 0;
        do
        {
            bodySize = 1; // data type constructor
            oldConstructorType = _constructorType;
            Element element = _first;
            while(element != null)
            {
                count++;
                bodySize += element.size();
                element = element.next();
            }
        }
        while (oldConstructorType != constructorType());

        if(isDescribed())
        {
            bodySize++; // 00 instruction
            if(count != 0)
            {
                count--;
            }
        }

        if(isElementOfArray())
        {
            ArrayElement parent = (ArrayElement)parent();
            if(parent.constructorType()==SMALL)
            {
                if(count<=255 && bodySize<=254)
                {
                    bodySize+=2;
                }
                else
                {
                    parent.setConstructorType(LARGE);
                    bodySize+=8;
                }
            }
            else
            {
                bodySize+=8;
            }
        }
        else
        {

            if(count<=255 && bodySize<=254)
            {
                bodySize+=3;
            }
            else
            {
                bodySize+=9;
            }

        }


        return bodySize;
    }

    @Override
    public Object[] getValue()
    {
        if(isDescribed())
        {
            DescribedType[] rVal = new DescribedType[(int) count()];
            Object descriptor = _first == null ? null : _first.getValue();
            Element element = _first == null ? null : _first.next();
            int i = 0;
            while(element != null)
            {
                rVal[i++] = new DescribedTypeImpl(descriptor, element.getValue());
                element = element.next();
            }
            return rVal;
        }
        else
        {
            Object[] rVal = new Object[(int) count()];
            Element element = _first;
            int i = 0;
            while (element!=null)
            {
                rVal[i++] = element.getValue();
                element = element.next();
            }
            return rVal;
        }
    }

    @Override
    public Data.DataType getDataType()
    {
        return Data.DataType.ARRAY;
    }

    @Override
    public int encode(ByteBuffer b)
    {
        int size = size();

        final int count = (int) count();

        if(b.remaining()>=size)
        {
            if(!isElementOfArray())
            {
                if(size>257 || count >255)
                {
                    b.put((byte)0xf0);
                    b.putInt(size-5);
                    b.putInt(count);
                }
                else
                {
                    b.put((byte)0xe0);
                    b.put((byte)(size-2));
                    b.put((byte)count);
                }
            }
            else
            {
                ArrayElement parent = (ArrayElement)parent();
                if(parent.constructorType()==SMALL)
                {
                    b.put((byte)(size-1));
                    b.put((byte)count);
                }
                else
                {
                    b.putInt(size-4);
                    b.putInt(count);
                }
            }
            Element element = _first;
            if(isDescribed())
            {
                b.put((byte)0);
                if(element == null)
                {
                    b.put((byte)0x40);
                }
                else
                {
                    element.encode(b);
                    element = element.next();
                }
            }
            switch(_arrayType)
            {
                case NULL:
                    b.put((byte)0x40);
                    break;
                case BOOL:
                    b.put((byte)0x56);
                    break;
                case UBYTE:
                    b.put((byte)0x50);
                    break;
                case BYTE:
                    b.put((byte)0x51);
                    break;
                case USHORT:
                    b.put((byte)0x60);
                    break;
                case SHORT:
                    b.put((byte)0x61);
                    break;
                case UINT:
                    switch (constructorType())
                    {
                        case TINY:
                            b.put((byte)0x43);
                            break;
                        case SMALL:
                            b.put((byte)0x52);
                            break;
                        case LARGE:
                            b.put((byte)0x70);
                            break;
                    }
                    break;
                case INT:
                    b.put(_constructorType == SMALL ? (byte)0x54 : (byte)0x71);
                    break;
                case CHAR:
                    b.put((byte)0x73);
                    break;
                case ULONG:
                    switch (constructorType())
                    {
                        case TINY:
                            b.put((byte)0x44);
                            break;
                        case SMALL:
                            b.put((byte)0x53);
                            break;
                        case LARGE:
                            b.put((byte)0x80);
                            break;
                    }
                    break;
                case LONG:
                    b.put(_constructorType == SMALL ? (byte)0x55 : (byte)0x81);
                    break;
                case TIMESTAMP:
                    b.put((byte)0x83);
                    break;
                case FLOAT:
                    b.put((byte)0x72);
                    break;
                case DOUBLE:
                    b.put((byte)0x82);
                    break;
                case DECIMAL32:
                    b.put((byte)0x74);
                    break;
                case DECIMAL64:
                    b.put((byte)0x84);
                    break;
                case DECIMAL128:
                    b.put((byte)0x94);
                    break;
                case UUID:
                    b.put((byte)0x98);
                    break;
                case BINARY:
                    b.put(_constructorType == SMALL ? (byte)0xa0 : (byte)0xb0);
                    break;
                case STRING:
                    b.put(_constructorType == SMALL ? (byte)0xa1 : (byte)0xb1);
                    break;
                case SYMBOL:
                    b.put(_constructorType == SMALL ? (byte)0xa3 : (byte)0xb3);
                    break;
                case ARRAY:
                    b.put(_constructorType == SMALL ? (byte)0xe0 : (byte)0xf0);
                    break;
                case LIST:
                    b.put(_constructorType == TINY ? (byte)0x45 :_constructorType == SMALL ? (byte)0xc0 : (byte)0xd0);
                    break;
                case MAP:
                    b.put(_constructorType == SMALL ? (byte)0xc1 : (byte)0xd1);
                    break;
            }
            while(element!=null)
            {
                element.encode(b);
                element = element.next();
            }
            return size;
        }
        else
        {
            return 0;
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
    public Element addChild(Element element)
    {
        if(isDescribed() || element.getDataType() == _arrayType)
        {
            _first = element;
            return element;
        }
        else
        {
            Element replacement = coerce(element);
            if(replacement != null)
            {
                _first = replacement;
                return replacement;
            }
            throw new IllegalArgumentException("Attempting to add instance of " + element.getDataType() + " to array of " + _arrayType);
        }
    }

    private Element coerce(Element element)
    {
        switch (_arrayType)
        {
        case INT:
            int i;
            switch (element.getDataType())
            {
            case BYTE:
                i = ((ByteElement)element).getValue().intValue();
                break;
            case SHORT:
                i = ((ShortElement)element).getValue().intValue();
                break;
            case LONG:
                i = ((LongElement)element).getValue().intValue();
                break;
            default:
                return null;
            }
            return new IntegerElement(element.parent(),element.prev(),i);

        case LONG:
            long l;
            switch (element.getDataType())
            {
            case BYTE:
                l = ((ByteElement)element).getValue().longValue();
                break;
            case SHORT:
                l = ((ShortElement)element).getValue().longValue();
                break;
            case INT:
                l = ((IntegerElement)element).getValue().longValue();
                break;
            default:
                return null;
            }
            return new LongElement(element.parent(),element.prev(),l);
        }
        return null;
    }

    @Override
    public Element checkChild(Element element)
    {
        if(element.getDataType() != _arrayType)
        {
            Element replacement = coerce(element);
            if(replacement != null)
            {
                return replacement;
            }
            throw new IllegalArgumentException("Attempting to add instance of " + element.getDataType() + " to array of " + _arrayType);
        }
        return element;
    }


    public long count()
    {
        int count = 0;
        Element elt = _first;
        while(elt != null)
        {
            count++;
            elt = elt.next();
        }
        if(isDescribed() && count != 0)
        {
            count--;
        }
        return count;
    }

    public boolean isDescribed()
    {
        return _described;
    }


    public Data.DataType getArrayDataType()
    {
        return _arrayType;
    }

    @Override
    String startSymbol() {
        return String.format("%s%s[", isDescribed() ? "D" : "", getArrayDataType());
    }

    @Override
    String stopSymbol() {
        return "]";
    }

}
