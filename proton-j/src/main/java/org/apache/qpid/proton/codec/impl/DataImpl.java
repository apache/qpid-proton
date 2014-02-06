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

import org.apache.qpid.proton.ProtonUnsupportedOperationException;
import org.apache.qpid.proton.amqp.*;
import org.apache.qpid.proton.codec.Data;


public class DataImpl implements Data
{

    private Element _first;
    private Element _current;
    private Element _parent;


    public DataImpl()
    {
    }

    @Override
    public void free()
    {
        _first = null;
        _current = null;

    }

    @Override
    public void clear()
    {
        _first=null;
        _current=null;
        _parent=null;
    }

    @Override
    public long size()
    {
        return _first == null ? 0 : _first.size();
    }

    @Override
    public void rewind()
    {
        _current = null;
        _parent = null;
    }

    @Override
    public DataType next()
    {
        Element next = _current == null ? (_parent == null ? _first : _parent.child()) : _current.next();

        if(next != null)
        {
            _current = next;
        }
        return next == null ? null : next.getDataType();
    }

    @Override
    public DataType prev()
    {
        Element prev = _current == null ? null : _current.prev();

        _current = prev;
        return prev == null ? null : prev.getDataType();
    }

    @Override
    public boolean enter()
    {
        if(_current != null && _current.canEnter())
        {

            _parent = _current;
            _current = null;
            return true;
        }
        return false;
    }

    @Override
    public boolean exit()
    {
        if(_parent != null)
        {
            Element parent = _parent;
            _current = parent;
            _parent = _current.parent();
            return true;

        }
        return false;
    }

    @Override
    public boolean lookup(String name)
    {
        // TODO
        throw new ProtonUnsupportedOperationException();

    }

    @Override
    public DataType type()
    {
        return _current == null ? null : _current.getDataType();
    }

    @Override
    public Binary encode()
    {
        int size = 0;
        Element elt = _first;
        while(elt != null)
        {
            size += elt.size();
            elt = elt.next();
        }
        byte[] data = new byte[size];
        ByteBuffer buf = ByteBuffer.wrap(data);
        encode(buf);

        return new Binary(data);
    }

    @Override
    public long encode(ByteBuffer buf)
    {
        Element elt = _first;
        int size = 0;
        while(elt != null )
        {
            final int eltSize = elt.size();
            if(eltSize <= buf.remaining())
            {
                size += elt.encode(buf);
            }
            else
            {
                size+= eltSize;
            }
            elt = elt.next();
        }
        return size;
    }

    @Override
    public long decode(ByteBuffer buf)
    {
        return DataDecoder.decode(buf, this);
    }


    private void putElement(Element element)
    {
        if(_first == null)
        {
            _first = element;
        }
        else
        {
            if(_current == null)
            {
                if (_parent == null) {
                    _first = _first.replaceWith(element);
                    element = _first;
                } else {
                    element = _parent.addChild(element);
                }
            }
            else
            {
                if(_parent!=null)
                {
                    element = _parent.checkChild(element);
                }
                _current.setNext(element);
            }
        }

        _current = element;
    }

    @Override
    public void putList()
    {
        putElement(new ListElement(_parent, _current));
    }

    @Override
    public void putMap()
    {
        putElement(new MapElement(_parent, _current));
    }

    @Override
    public void putArray(boolean described, DataType type)
    {
        putElement(new ArrayElement(_parent, _current, described, type));

    }

    @Override
    public void putDescribed()
    {
        putElement(new DescribedTypeElement(_parent, _current));
    }

    @Override
    public void putNull()
    {
        putElement(new NullElement(_parent, _current));

    }

    @Override
    public void putBoolean(boolean b)
    {
        putElement(new BooleanElement(_parent, _current, b));
    }

    @Override
    public void putUnsignedByte(UnsignedByte ub)
    {
        putElement(new UnsignedByteElement(_parent, _current, ub));

    }

    @Override
    public void putByte(byte b)
    {
        putElement(new ByteElement(_parent, _current, b));
    }

    @Override
    public void putUnsignedShort(UnsignedShort us)
    {
        putElement(new UnsignedShortElement(_parent, _current, us));

    }

    @Override
    public void putShort(short s)
    {
        putElement(new ShortElement(_parent, _current, s));
    }

    @Override
    public void putUnsignedInteger(UnsignedInteger ui)
    {
        putElement(new UnsignedIntegerElement(_parent, _current, ui));
    }

    @Override
    public void putInt(int i)
    {
        putElement(new IntegerElement(_parent, _current, i));
    }

    @Override
    public void putChar(int c)
    {
        putElement(new CharElement(_parent, _current, c));
    }

    @Override
    public void putUnsignedLong(UnsignedLong ul)
    {
        putElement(new UnsignedLongElement(_parent, _current, ul));
    }

    @Override
    public void putLong(long l)
    {
        putElement(new LongElement(_parent, _current, l));
    }

    @Override
    public void putTimestamp(Date t)
    {
        putElement(new TimestampElement(_parent,_current,t));
    }

    @Override
    public void putFloat(float f)
    {
        putElement(new FloatElement(_parent,_current,f));
    }

    @Override
    public void putDouble(double d)
    {
        putElement(new DoubleElement(_parent,_current,d));
    }

    @Override
    public void putDecimal32(Decimal32 d)
    {
        putElement(new Decimal32Element(_parent,_current,d));
    }

    @Override
    public void putDecimal64(Decimal64 d)
    {
        putElement(new Decimal64Element(_parent,_current,d));
    }

    @Override
    public void putDecimal128(Decimal128 d)
    {
        putElement(new Decimal128Element(_parent,_current,d));
    }

    @Override
    public void putUUID(UUID u)
    {
        putElement(new UUIDElement(_parent,_current,u));
    }

    @Override
    public void putBinary(Binary bytes)
    {
        putElement(new BinaryElement(_parent, _current, bytes));
    }

    @Override
    public void putBinary(byte[] bytes)
    {
        putBinary(new Binary(bytes));
    }

    @Override
    public void putString(String string)
    {
        putElement(new StringElement(_parent,_current,string));
    }

    @Override
    public void putSymbol(Symbol symbol)
    {
        putElement(new SymbolElement(_parent,_current,symbol));
    }

    @Override
    public void putObject(Object o)
    {
        if(o == null)
        {
            putNull();
        }
        else if(o instanceof Boolean)
        {
            putBoolean((Boolean) o);
        }
        else if(o instanceof UnsignedByte)
        {
            putUnsignedByte((UnsignedByte)o);
        }
        else if(o instanceof Byte)
        {
            putByte((Byte)o);
        }
        else if(o instanceof UnsignedShort)
        {
            putUnsignedShort((UnsignedShort)o);
        }
        else if(o instanceof Short)
        {
            putShort((Short)o);
        }
        else if(o instanceof UnsignedInteger)
        {
            putUnsignedInteger((UnsignedInteger)o);
        }
        else if(o instanceof Integer)
        {
            putInt((Integer)o);
        }
        else if(o instanceof Character)
        {
            putChar((Character)o);
        }
        else if(o instanceof UnsignedLong)
        {
            putUnsignedLong((UnsignedLong)o);
        }
        else if(o instanceof Long)
        {
            putLong((Long)o);
        }
        else if(o instanceof Date)
        {
            putTimestamp((Date)o);
        }
        else if(o instanceof Float)
        {
            putFloat((Float)o);
        }
        else if(o instanceof Double)
        {
            putDouble((Double)o);
        }
        else if(o instanceof Decimal32)
        {
            putDecimal32((Decimal32)o);
        }
        else if(o instanceof Decimal64)
        {
            putDecimal64((Decimal64)o);
        }
        else if(o instanceof Decimal128)
        {
            putDecimal128((Decimal128)o);
        }
        else if(o instanceof UUID)
        {
            putUUID((UUID)o);
        }
        else if(o instanceof Binary)
        {
            putBinary((Binary)o);
        }
        else if(o instanceof String)
        {
            putString((String)o);
        }
        else if(o instanceof Symbol)
        {
            putSymbol((Symbol)o);
        }
        else if(o instanceof DescribedType)
        {
            putDescribedType((DescribedType)o);
        }
        else if(o instanceof Object[])
        {
            putJavaArray((Object[]) o);
        }
        else if(o instanceof List)
        {
            putJavaList((List)o);
        }
        else if(o instanceof Map)
        {
            putJavaMap((Map)o);
        }
        else
        {
            throw new IllegalArgumentException("Unknown type " + o.getClass().getSimpleName());
        }
    }

    @Override
    public void putJavaMap(Map<Object, Object> map)
    {
        putMap();
        enter();
        for(Map.Entry<Object, Object> entry : map.entrySet())
        {
            putObject(entry.getKey());
            putObject(entry.getValue());
        }
        exit();

    }

    @Override
    public void putJavaList(List<Object> list)
    {
        putList();
        enter();
        for(Object o : list)
        {
            putObject(o);
        }
        exit();
    }

    @Override
    public void putJavaArray(Object[] array)
    {
        // TODO
        throw new ProtonUnsupportedOperationException();
    }

    @Override
    public void putDescribedType(DescribedType dt)
    {
        putElement(new DescribedTypeElement(_parent,_current));
        enter();
        putObject(dt.getDescriptor());
        putObject(dt.getDescribed());
        exit();
    }

    @Override
    public long getList()
    {
        if(_current instanceof ListElement)
        {
            return ((ListElement)_current).count();
        }
        throw new IllegalStateException("Current value not list");
    }

    @Override
    public long getMap()
    {
        if(_current instanceof MapElement)
        {
            return ((MapElement)_current).count();
        }
        throw new IllegalStateException("Current value not map");
    }

    @Override
    public long getArray()
    {
        if(_current instanceof ArrayElement)
        {
            return ((ArrayElement)_current).count();
        }
        throw new IllegalStateException("Current value not array");
    }

    @Override
    public boolean isArrayDescribed()
    {
        if(_current instanceof ArrayElement)
        {
            return ((ArrayElement)_current).isDescribed();
        }
        throw new IllegalStateException("Current value not array");
    }

    @Override
    public DataType getArrayType()
    {
        if(_current instanceof ArrayElement)
        {
            return ((ArrayElement)_current).getArrayDataType();
        }
        throw new IllegalStateException("Current value not array");
    }

    @Override
    public boolean isDescribed()
    {
        return _current != null && _current.getDataType() == DataType.DESCRIBED;
    }

    @Override
    public boolean isNull()
    {
        return _current != null && _current.getDataType() == DataType.NULL;
    }

    @Override
    public boolean getBoolean()
    {
        if(_current instanceof BooleanElement)
        {
            return ((BooleanElement)_current).getValue();
        }
        throw new IllegalStateException("Current value not boolean");
    }

    @Override
    public UnsignedByte getUnsignedByte()
    {

        if(_current instanceof UnsignedByteElement)
        {
            return ((UnsignedByteElement)_current).getValue();
        }
        throw new IllegalStateException("Current value not unsigned byte");
    }

    @Override
    public byte getByte()
    {
        if(_current instanceof ByteElement)
        {
            return ((ByteElement)_current).getValue();
        }
        throw new IllegalStateException("Current value not byte");
    }

    @Override
    public UnsignedShort getUnsignedShort()
    {
        if(_current instanceof UnsignedShortElement)
        {
            return ((UnsignedShortElement)_current).getValue();
        }
        throw new IllegalStateException("Current value not unsigned short");
    }

    @Override
    public short getShort()
    {
        if(_current instanceof ShortElement)
        {
            return ((ShortElement)_current).getValue();
        }
        throw new IllegalStateException("Current value not short");
    }

    @Override
    public UnsignedInteger getUnsignedInteger()
    {
        if(_current instanceof UnsignedIntegerElement)
        {
            return ((UnsignedIntegerElement)_current).getValue();
        }
        throw new IllegalStateException("Current value not unsigned integer");
    }

    @Override
    public int getInt()
    {
        if(_current instanceof IntegerElement)
        {
            return ((IntegerElement)_current).getValue();
        }
        throw new IllegalStateException("Current value not integer");
    }

    @Override
    public int getChar()
    {
        if(_current instanceof CharElement)
        {
            return ((CharElement)_current).getValue();
        }
        throw new IllegalStateException("Current value not char");
    }

    @Override
    public UnsignedLong getUnsignedLong()
    {
        if(_current instanceof UnsignedLongElement)
        {
            return ((UnsignedLongElement)_current).getValue();
        }
        throw new IllegalStateException("Current value not unsigned long");
    }

    @Override
    public long getLong()
    {
        if(_current instanceof LongElement)
        {
            return ((LongElement)_current).getValue();
        }
        throw new IllegalStateException("Current value not long");
    }

    @Override
    public Date getTimestamp()
    {
        if(_current instanceof TimestampElement)
        {
            return ((TimestampElement)_current).getValue();
        }
        throw new IllegalStateException("Current value not timestamp");
    }

    @Override
    public float getFloat()
    {
        if(_current instanceof FloatElement)
        {
            return ((FloatElement)_current).getValue();
        }
        throw new IllegalStateException("Current value not float");
    }

    @Override
    public double getDouble()
    {
        if(_current instanceof DoubleElement)
        {
            return ((DoubleElement)_current).getValue();
        }
        throw new IllegalStateException("Current value not double");
    }

    @Override
    public Decimal32 getDecimal32()
    {
        if(_current instanceof Decimal32Element)
        {
            return ((Decimal32Element)_current).getValue();
        }
        throw new IllegalStateException("Current value not decimal32");
    }

    @Override
    public Decimal64 getDecimal64()
    {
        if(_current instanceof Decimal64Element)
        {
            return ((Decimal64Element)_current).getValue();
        }
        throw new IllegalStateException("Current value not decimal32");
    }

    @Override
    public Decimal128 getDecimal128()
    {
        if(_current instanceof Decimal128Element)
        {
            return ((Decimal128Element)_current).getValue();
        }
        throw new IllegalStateException("Current value not decimal32");
    }

    @Override
    public UUID getUUID()
    {
        if(_current instanceof UUIDElement)
        {
            return ((UUIDElement)_current).getValue();
        }
        throw new IllegalStateException("Current value not uuid");
    }

    @Override
    public Binary getBinary()
    {
        if(_current instanceof BinaryElement)
        {
            return ((BinaryElement)_current).getValue();
        }
        throw new IllegalStateException("Current value not binary");
    }

    @Override
    public String getString()
    {
        if (_current instanceof StringElement)
        {
            return ((StringElement) _current).getValue();
        }
        throw new IllegalStateException("Current value not string");
    }

    @Override
    public Symbol getSymbol()
    {
        if(_current instanceof SymbolElement)
        {
            return ((SymbolElement)_current).getValue();
        }
        throw new IllegalStateException("Current value not symbol");
    }

    @Override
    public Object getObject()
    {
        return _current == null ? null : _current.getValue();
    }

    @Override
    public Map<Object, Object> getJavaMap()
    {
        if(_current instanceof MapElement)
        {
            return ((MapElement)_current).getValue();
        }
        throw new IllegalStateException("Current value not map");
    }

    @Override
    public List<Object> getJavaList()
    {
        if(_current instanceof ListElement)
        {
            return ((ListElement)_current).getValue();
        }
        throw new IllegalStateException("Current value not list");
    }

    @Override
    public Object[] getJavaArray()
    {
        if(_current instanceof ArrayElement)
        {
            return ((ArrayElement)_current).getValue();
        }
        throw new IllegalStateException("Current value not array");
    }

    @Override
    public DescribedType getDescribedType()
    {
        if(_current instanceof DescribedTypeElement)
        {
            return ((DescribedTypeElement)_current).getValue();
        }
        throw new IllegalStateException("Current value not described type");
    }

    @Override
    public void copy(Data src)
    {
        // TODO

        throw new ProtonUnsupportedOperationException();
    }

    @Override
    public void append(Data src)
    {
        // TODO

        throw new ProtonUnsupportedOperationException();
    }

    @Override
    public void appendn(Data src, int limit)
    {
        // TODO

        throw new ProtonUnsupportedOperationException();
    }

    @Override
    public void narrow()
    {
        // TODO

        throw new ProtonUnsupportedOperationException();
    }

    @Override
    public void widen()
    {
        // TODO

        throw new ProtonUnsupportedOperationException();
    }


    @Override
    public String format()
    {
        StringBuilder sb = new StringBuilder();
        Element el = _first;
        boolean first = true;
        while (el != null) {
            if (first) {
                first = false;
            } else {
                sb.append(", ");
            }
            el.render(sb);
            el = el.next();
        }

        return sb.toString();
    }

    private void render(StringBuilder sb, Element el)
    {
        if (el == null) return;
        sb.append("    ").append(el).append("\n");
        if (el.canEnter()) {
            render(sb, el.child());
        }
        render(sb, el.next());
    }

    @Override
    public String toString()
    {
        StringBuilder sb = new StringBuilder();
        render(sb, _first);
        return String.format("Data[current=%h, parent=%h]{\n%s}",
                             System.identityHashCode(_current),
                             System.identityHashCode(_parent),
                             sb);
    }

}
