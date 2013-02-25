/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License
 Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing

 * software distributed under the License is distributed on an
 * "AS IS" BASIS
 WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND
 either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */
package org.apache.qpid.proton.codec.jni;

import java.lang.reflect.Array;
import java.math.BigInteger;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.Date;
import java.util.EnumMap;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import org.apache.qpid.proton.ProtonCEquivalent;
import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.Decimal128;
import org.apache.qpid.proton.amqp.Decimal32;
import org.apache.qpid.proton.amqp.Decimal64;
import org.apache.qpid.proton.amqp.DescribedType;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedByte;
import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.UnsignedShort;
import org.apache.qpid.proton.codec.Data;
import org.apache.qpid.proton.jni.Proton;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_data_t;
import org.apache.qpid.proton.jni.pn_bytes_t;
import org.apache.qpid.proton.jni.pn_decimal128_t;
import org.apache.qpid.proton.jni.pn_type_t;
import org.apache.qpid.proton.jni.pn_uuid_t;

public class JNIData implements Data
{
    public static final Charset UTF8_CHARSET = Charset.forName("UTF-8");
    public static final Charset ASCII_CHARSET = Charset.forName("US-ASCII");


    private final SWIGTYPE_p_pn_data_t _impl;
    private static final Map<DataType, Class> CLASSMAP = new EnumMap<DataType,Class>(DataType.class);

    static
    {
        CLASSMAP.put(DataType.NULL, Void.class);
        CLASSMAP.put(DataType.BOOL, Boolean.class);
        CLASSMAP.put(DataType.UBYTE, UnsignedByte.class);
        CLASSMAP.put(DataType.BYTE, Byte.class);
        CLASSMAP.put(DataType.USHORT, UnsignedShort.class);
        CLASSMAP.put(DataType.SHORT, Short.class);
        CLASSMAP.put(DataType.UINT, UnsignedInteger.class);
        CLASSMAP.put(DataType.INT, Integer.class);
        CLASSMAP.put(DataType.CHAR, Character.class);
        CLASSMAP.put(DataType.ULONG, UnsignedLong.class);
        CLASSMAP.put(DataType.LONG, Long.class);
        CLASSMAP.put(DataType.TIMESTAMP, Date.class);
        CLASSMAP.put(DataType.FLOAT, Float.class);
        CLASSMAP.put(DataType.DOUBLE, Double.class);
        CLASSMAP.put(DataType.DECIMAL32, Decimal32.class);
        CLASSMAP.put(DataType.DECIMAL64, Decimal64.class);
        CLASSMAP.put(DataType.DECIMAL128, Decimal128.class);
        CLASSMAP.put(DataType.UUID, UUID.class);
        CLASSMAP.put(DataType.BINARY, Binary.class);
        CLASSMAP.put(DataType.STRING, String.class);
        CLASSMAP.put(DataType.SYMBOL, Symbol.class);
        CLASSMAP.put(DataType.DESCRIBED, DescribedType.class);
        CLASSMAP.put(DataType.ARRAY, Object[].class);
        CLASSMAP.put(DataType.LIST, List.class);
        CLASSMAP.put(DataType.MAP, Map.class);
    }

    private static final Map<Class, DataType> REVERSE_CLASSMAP = new HashMap<Class, DataType>();

    static
    {
        for(Map.Entry<DataType, Class> entry : CLASSMAP.entrySet())
        {
            REVERSE_CLASSMAP.put(entry.getValue(), entry.getKey());
        }
    }

    public JNIData(long capacity)
    {
        _impl = Proton.pn_data(capacity);
    }

    public JNIData(SWIGTYPE_p_pn_data_t impl)
    {
        _impl = impl;
    }

    @Override
    @ProtonCEquivalent("pn_data_free")
    public void free()
    {
        //TODO
        Proton.pn_data_free(_impl);

    }

    @Override
    @ProtonCEquivalent("pn_data_clear")
    public void clear()
    {
        //TODO
        Proton.pn_data_clear(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_data_size")
    public long size()
    {
        return Proton.pn_data_size(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_data_rewind")
    public void rewind()
    {
        Proton.pn_data_rewind(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_data_next")
    public DataType next()
    {
        final boolean next = Proton.pn_data_next(_impl);
        return next ? type() : null;
    }

    @Override
    @ProtonCEquivalent("pn_data_prev")
    public DataType prev()
    {
        return Proton.pn_data_prev(_impl) ? type() : null;
    }

    @Override
    @ProtonCEquivalent("pn_data_next")
    public boolean enter()
    {
        return Proton.pn_data_enter(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_data_exit")
    public boolean exit()
    {
        return Proton.pn_data_exit(_impl);
    }

    static final Map<pn_type_t, DataType> TYPEMAP = new HashMap<pn_type_t, DataType>();
    static final Map<DataType, pn_type_t> REVERSE_TYPEMAP = new EnumMap<DataType,pn_type_t>(DataType.class);
    static
    {
        TYPEMAP.put(pn_type_t.PN_NULL, DataType.NULL);
        TYPEMAP.put(pn_type_t.PN_BOOL, DataType.BOOL);
        TYPEMAP.put(pn_type_t.PN_UBYTE, DataType.UBYTE);
        TYPEMAP.put(pn_type_t.PN_BYTE, DataType.BYTE);
        TYPEMAP.put(pn_type_t.PN_USHORT, DataType.USHORT);
        TYPEMAP.put(pn_type_t.PN_SHORT, DataType.SHORT);
        TYPEMAP.put(pn_type_t.PN_UINT, DataType.UINT);
        TYPEMAP.put(pn_type_t.PN_INT, DataType.INT);
        TYPEMAP.put(pn_type_t.PN_CHAR, DataType.CHAR);
        TYPEMAP.put(pn_type_t.PN_ULONG, DataType.ULONG);
        TYPEMAP.put(pn_type_t.PN_LONG, DataType.LONG);
        TYPEMAP.put(pn_type_t.PN_TIMESTAMP, DataType.TIMESTAMP);
        TYPEMAP.put(pn_type_t.PN_FLOAT, DataType.FLOAT);
        TYPEMAP.put(pn_type_t.PN_DOUBLE, DataType.DOUBLE);
        TYPEMAP.put(pn_type_t.PN_DECIMAL32, DataType.DECIMAL32);
        TYPEMAP.put(pn_type_t.PN_DECIMAL64, DataType.DECIMAL64);
        TYPEMAP.put(pn_type_t.PN_DECIMAL128, DataType.DECIMAL128);
        TYPEMAP.put(pn_type_t.PN_UUID, DataType.UUID);
        TYPEMAP.put(pn_type_t.PN_BINARY, DataType.BINARY);
        TYPEMAP.put(pn_type_t.PN_STRING, DataType.STRING);
        TYPEMAP.put(pn_type_t.PN_SYMBOL, DataType.SYMBOL);
        TYPEMAP.put(pn_type_t.PN_DESCRIBED, DataType.DESCRIBED);
        TYPEMAP.put(pn_type_t.PN_ARRAY, DataType.ARRAY);
        TYPEMAP.put(pn_type_t.PN_LIST, DataType.LIST);
        TYPEMAP.put(pn_type_t.PN_MAP, DataType.MAP);

        REVERSE_TYPEMAP.put(DataType.NULL, pn_type_t.PN_NULL);
        REVERSE_TYPEMAP.put(DataType.BOOL, pn_type_t.PN_BOOL);
        REVERSE_TYPEMAP.put(DataType.UBYTE, pn_type_t.PN_UBYTE);
        REVERSE_TYPEMAP.put(DataType.BYTE, pn_type_t.PN_BYTE);
        REVERSE_TYPEMAP.put(DataType.USHORT, pn_type_t.PN_USHORT);
        REVERSE_TYPEMAP.put(DataType.SHORT, pn_type_t.PN_SHORT);
        REVERSE_TYPEMAP.put(DataType.UINT, pn_type_t.PN_UINT);
        REVERSE_TYPEMAP.put(DataType.INT, pn_type_t.PN_INT);
        REVERSE_TYPEMAP.put(DataType.CHAR, pn_type_t.PN_CHAR);
        REVERSE_TYPEMAP.put(DataType.ULONG, pn_type_t.PN_ULONG);
        REVERSE_TYPEMAP.put(DataType.LONG, pn_type_t.PN_LONG);
        REVERSE_TYPEMAP.put(DataType.TIMESTAMP, pn_type_t.PN_TIMESTAMP);
        REVERSE_TYPEMAP.put(DataType.FLOAT, pn_type_t.PN_FLOAT);
        REVERSE_TYPEMAP.put(DataType.DOUBLE, pn_type_t.PN_DOUBLE);
        REVERSE_TYPEMAP.put(DataType.DECIMAL32, pn_type_t.PN_DECIMAL32);
        REVERSE_TYPEMAP.put(DataType.DECIMAL64, pn_type_t.PN_DECIMAL64);
        REVERSE_TYPEMAP.put(DataType.DECIMAL128, pn_type_t.PN_DECIMAL128);
        REVERSE_TYPEMAP.put(DataType.UUID, pn_type_t.PN_UUID);
        REVERSE_TYPEMAP.put(DataType.BINARY, pn_type_t.PN_BINARY);
        REVERSE_TYPEMAP.put(DataType.STRING, pn_type_t.PN_STRING);
        REVERSE_TYPEMAP.put(DataType.SYMBOL, pn_type_t.PN_SYMBOL);
        REVERSE_TYPEMAP.put(DataType.DESCRIBED, pn_type_t.PN_DESCRIBED);
        REVERSE_TYPEMAP.put(DataType.ARRAY, pn_type_t.PN_ARRAY);
        REVERSE_TYPEMAP.put(DataType.LIST, pn_type_t.PN_LIST);
        REVERSE_TYPEMAP.put(DataType.MAP, pn_type_t.PN_MAP);

    }


    @Override
    @ProtonCEquivalent("pn_data_type")
    public DataType type()
    {
        try
        {
            pn_type_t dataType = Proton.pn_data_type(_impl);
            final DataType type = TYPEMAP.get(dataType);
            return type;
        }
        catch(IllegalArgumentException e)
        {
            return null;
        }


    }

    @Override
    public Binary encode()
    {
        int size = 1024;
        long rval = Proton.PN_OVERFLOW;
        ByteBuffer buf = null;
        while(rval == Proton.PN_OVERFLOW)
        {
            buf = ByteBuffer.allocate(size);
            rval = encode(buf);
        }
        return new Binary(buf.array(), buf.arrayOffset(), buf.arrayOffset()+(int)rval);
    }


    @Override
    @ProtonCEquivalent("pn_data_encode")
    public long encode(final ByteBuffer buf)
    {
        return Proton.pn_data_encode(_impl, buf);
    }

    @Override
    @ProtonCEquivalent("pn_data_decode")
    public long decode(final ByteBuffer buf)
    {
        int rval = Proton.pn_data_decode(_impl, buf);
        return rval;
    }

    @Override
    @ProtonCEquivalent("pn_data_put_list")
    public void putList()
    {
        Proton.pn_data_put_list(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_data_put_map")
    public void putMap()
    {
        Proton.pn_data_put_map(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_data_put_array")
    public void putArray(final boolean described, final DataType type)
    {
        Proton.pn_data_put_array(_impl, described, REVERSE_TYPEMAP.get(type));
    }

    @Override
    @ProtonCEquivalent("pn_data_put_described")
    public void putDescribed()
    {
        Proton.pn_data_put_described(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_data_put_null")
    public void putNull()
    {
        Proton.pn_data_put_null(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_data_put_bool")
    public void putBoolean(final boolean b)
    {
        Proton.pn_data_put_bool(_impl, b);
    }

    @Override
    @ProtonCEquivalent("pn_data_put_ubyte")
    public void putUnsignedByte(final UnsignedByte ub)
    {
        Proton.pn_data_put_ubyte(_impl, ub.shortValue());
    }

    @Override
    @ProtonCEquivalent("pn_data_put_byte")
    public void putByte(final byte b)
    {
        Proton.pn_data_put_byte(_impl, b);
    }

    @Override
    @ProtonCEquivalent("pn_data_put_ushort")
    public void putUnsignedShort(final UnsignedShort us)
    {
        Proton.pn_data_put_ushort(_impl, us.intValue());
    }

    @Override
    @ProtonCEquivalent("pn_data_put_short")
    public void putShort(final short s)
    {
        Proton.pn_data_put_short(_impl, s);
    }

    @Override
    @ProtonCEquivalent("pn_data_put_uint")
    public void putUnsignedInteger(final UnsignedInteger ui)
    {
        Proton.pn_data_put_uint(_impl, ui.intValue());
    }

    @Override
    @ProtonCEquivalent("pn_data_put_int")
    public void putInt(final int i)
    {
        Proton.pn_data_put_int(_impl, i);
    }


    @Override
    @ProtonCEquivalent("pn_data_put_char")
    public void putChar(final int c)
    {
        Proton.pn_data_put_char(_impl, c);
    }

    @Override
    @ProtonCEquivalent("pn_data_put_ulong")
    public void putUnsignedLong(final UnsignedLong ul)
    {
        Proton.pn_data_put_ulong(_impl, ul.bigIntegerValue());
    }

    @Override
    @ProtonCEquivalent("pn_data_put_long")
    public void putLong(final long l)
    {
        Proton.pn_data_put_long(_impl, l);
    }

    @Override
    @ProtonCEquivalent("pn_data_put_timestamp")
    public void putTimestamp(final Date t)
    {
        Proton.pn_data_put_timestamp(_impl, t.getTime());
    }

    @Override
    @ProtonCEquivalent("pn_data_put_float")
    public void putFloat(final float f)
    {
        Proton.pn_data_put_float(_impl, f);
    }

    @Override
    @ProtonCEquivalent("pn_data_put_double")
    public void putDouble(final double d)
    {
        Proton.pn_data_put_double(_impl, d);
    }

    @Override
    @ProtonCEquivalent("pn_data_put_decimal32")
    public void putDecimal32(final Decimal32 d)
    {
        Proton.pn_data_put_decimal32(_impl, d.getBits());
    }

    @Override
    @ProtonCEquivalent("pn_data_put_decimal64")
    public void putDecimal64(final Decimal64 d)
    {
        BigInteger bi = BigInteger.valueOf(d.getBits());
        Proton.pn_data_put_decimal64(_impl, bi);
    }

    @Override
    @ProtonCEquivalent("pn_data_put_decimal128")
    public void putDecimal128(final Decimal128 d)
    {
        byte[] data = new byte[16];
        ByteBuffer buf = ByteBuffer.wrap(data);
        buf.putLong(d.getMostSignificantBits());
        buf.putLong(d.getLeastSignificantBits());
        pn_decimal128_t decimal128_t = new pn_decimal128_t();
        decimal128_t.setBytes(data);
        Proton.pn_data_put_decimal128(_impl,decimal128_t);
    }

    @Override
    @ProtonCEquivalent("pn_data_put_uuid")
    public void putUUID(final UUID u)
    {
        final pn_uuid_t uuid = new pn_uuid_t();
        byte[] data = new byte[16];
        ByteBuffer buf = ByteBuffer.wrap(data);
        buf.putLong(u.getMostSignificantBits());
        buf.putLong(u.getLeastSignificantBits());
        uuid.setBytes(data);

        Proton.pn_data_put_uuid(_impl, uuid);

        //TODO
    }

    @Override
    public void putBinary(final byte[] bytes)
    {
        putBinary(new Binary(bytes));
    }

    @Override
    @ProtonCEquivalent("pn_data_put_binary")
    public void putBinary(final Binary bytes)
    {
        pn_bytes_t bytes_t = new pn_bytes_t();
        byte[] data = new byte[bytes.getLength()];
        System.arraycopy(bytes.getArray(),bytes.getArrayOffset(),data,0,bytes.getLength());
        Proton.pn_bytes_from_array(bytes_t, data);

        Proton.pn_data_put_binary(_impl, bytes_t);
        //TODO
    }

    @Override
    @ProtonCEquivalent("pn_data_put_string")
    public void putString(final String string)
    {
        byte[] data = string.getBytes(UTF8_CHARSET);
        pn_bytes_t bytes_t = new pn_bytes_t();
        Proton.pn_bytes_from_array(bytes_t, data);
        Proton.pn_data_put_string(_impl, bytes_t);

        //TODO
    }

    @Override
    @ProtonCEquivalent("pn_data_put_symbol")
    public void putSymbol(final Symbol symbol)
    {
        byte[] data = symbol.toString().getBytes(ASCII_CHARSET);
        pn_bytes_t bytes_t = new pn_bytes_t();
        Proton.pn_bytes_from_array(bytes_t, data);
        Proton.pn_data_put_symbol(_impl, bytes_t);
        //TODO
    }

    @Override
    @ProtonCEquivalent("pn_data_get_list")
    public long getList()
    {
        return Proton.pn_data_get_list(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_data_get_map")
    public long getMap()
    {
        return Proton.pn_data_get_map(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_data_get_array")
    public long getArray()
    {
        return Proton.pn_data_get_array(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_data_is_array_described")
    public boolean isArrayDescribed()
    {
        return Proton.pn_data_is_array_described(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_data_get_array_type")
    public DataType getArrayType()
    {
        return TYPEMAP.get(Proton.pn_data_get_array_type(_impl));
    }

    @Override
    @ProtonCEquivalent("pn_data_is_described")
    public boolean isDescribed()
    {
        return Proton.pn_data_is_described(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_data_is_null")
    public boolean isNull()
    {
        return Proton.pn_data_is_null(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_data_get_bool")
    public boolean getBoolean()
    {
        return Proton.pn_data_get_bool(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_data_get_ubyte")
    public UnsignedByte getUnsignedByte()
    {
        return UnsignedByte.valueOf((byte) Proton.pn_data_get_ubyte(_impl));
    }

    @Override
    @ProtonCEquivalent("pn_data_get_byte")
    public byte getByte()
    {
        return Proton.pn_data_get_byte(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_data_get_ushort")
    public UnsignedShort getUnsignedShort()
    {
        return UnsignedShort.valueOf((short) Proton.pn_data_get_ushort(_impl));
    }

    @Override
    @ProtonCEquivalent("pn_data_get_short")
    public short getShort()
    {
        return Proton.pn_data_get_short(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_data_get_uint")
    public UnsignedInteger getUnsignedInteger()
    {
        return UnsignedInteger.valueOf(Proton.pn_data_get_uint(_impl));
    }

    @Override
    @ProtonCEquivalent("pn_data_get_int")
    public int getInt()
    {
        return Proton.pn_data_get_int(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_data_get_char")
    public int getChar()
    {
        final int c = (int) Proton.pn_data_get_char(_impl);
        return c;
    }

    @Override
    @ProtonCEquivalent("pn_data_get_ulong")
    public UnsignedLong getUnsignedLong()
    {
        return UnsignedLong.valueOf(Proton.pn_data_get_ulong(_impl).toString());
    }

    @Override
    @ProtonCEquivalent("pn_data_get_long")
    public long getLong()
    {
        return Proton.pn_data_get_long(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_data_get_timestamp")
    public Date getTimestamp()
    {
        return new Date(Proton.pn_data_get_timestamp(_impl));
    }

    @Override
    @ProtonCEquivalent("pn_data_get_float")
    public float getFloat()
    {
        return Proton.pn_data_get_float(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_data_get_double")
    public double getDouble()
    {
        return Proton.pn_data_get_double(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_data_get_decimal32")
    public Decimal32 getDecimal32()
    {
        return new Decimal32((int)Proton.pn_data_get_decimal32(_impl));
    }

    @Override
    @ProtonCEquivalent("pn_data_get_decimal64")
    public Decimal64 getDecimal64()
    {
        return new Decimal64(Proton.pn_data_get_decimal64(_impl).longValue());
    }

    @Override
    @ProtonCEquivalent("pn_data_get_decimal128")
    public Decimal128 getDecimal128()
    {
        pn_decimal128_t b = Proton.pn_data_get_decimal128(_impl);
        ByteBuffer buf = ByteBuffer.wrap(b.getBytes());
        return new Decimal128(buf.getLong(),buf.getLong());
    }

    @Override
    @ProtonCEquivalent("pn_data_get_uuid")
    public UUID getUUID()
    {
        pn_uuid_t u = Proton.pn_data_get_uuid(_impl);
        ByteBuffer buf = ByteBuffer.wrap(u.getBytes());
        long msb = buf.getLong();
        long lsb = buf.getLong();

        final UUID uuid = new UUID(msb, lsb);
        return uuid;
    }

    @Override
    @ProtonCEquivalent("pn_data_get_binary")
    public Binary getBinary()
    {
        pn_bytes_t b = Proton.pn_data_get_binary(_impl);

        return b == null ? null : new Binary(Proton.pn_bytes_to_array(b));
    }

    @Override
    @ProtonCEquivalent("pn_data_get_string")
    public String getString()
    {
        pn_bytes_t b = Proton.pn_data_get_string(_impl);
        return b == null ? null : new String(Proton.pn_bytes_to_array(b), UTF8_CHARSET);
    }

    @Override
    @ProtonCEquivalent("pn_data_get_symbol")
    public Symbol getSymbol()
    {
        pn_bytes_t b = Proton.pn_data_get_symbol(_impl);
        return b == null ? null : Symbol.valueOf(new String(Proton.pn_bytes_to_array(b), ASCII_CHARSET));
    }

    @Override
    public void putObject(final Object o)
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
//      TODO-          ARRAY,
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
//
        //TODO
    }

    @Override
    public void putJavaMap(final Map<Object, Object> map)
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
    public void putJavaList(final List<Object> list)
    {
        putList();
        enter();
        for(Object element : list)
        {
            putObject(element);
        }
        exit();
    }

    @Override
    public void putJavaArray(final Object[] array)
    {
        //TODO
        Class eltClazz = array.getClass().getComponentType();
        boolean described = eltClazz.isInstance(DescribedType.class);
        putArray(described, getType(described ? ((DescribedType) array[0]).getDescribed().getClass() : eltClazz));
        enter();
        for(Object elt : array)
        {
            putObject(elt);
        }
        exit();
    }



    private DataType getType(final Class aClass)
    {
        if(aClass.isInstance(Map.class))
        {
            return DataType.MAP;
        }
        if(aClass.isInstance(List.class))
        {
            return DataType.LIST;
        }
        DataType t = REVERSE_CLASSMAP.get(aClass);
        if(t != null)
        {
            return t;
        }
        return null;  //TODO
    }

    @Override
    public void putDescribedType(final DescribedType dt)
    {
        putDescribed();
        enter();
        putObject(dt.getDescriptor());
        putObject(dt.getDescribed());
        exit();
    }

    @Override
    public Object getObject()
    {

        final DataType type = type();
        if(type == null)
        {
            return null;
        }

        switch(type)
        {
            case NULL:
                return null;

            case BOOL:
                return getBoolean();

            case UBYTE:
                return getUnsignedByte();

            case BYTE:
                return getByte();

            case USHORT:
                return getUnsignedShort();

            case SHORT:
                return getShort();

            case UINT:
                return getUnsignedInteger();

            case INT:
                return getInt();

            case CHAR:
                return getChar();

            case ULONG:
                return getUnsignedLong();

            case LONG:
                return getLong();

            case TIMESTAMP:
                return getTimestamp();

            case FLOAT:
                return getFloat();

            case DOUBLE:
                return getDouble();

            case DECIMAL32:
                return getDecimal32();

            case DECIMAL64:
                return getDecimal64();

            case DECIMAL128:
                return getDecimal128();

            case UUID:
                return getUUID();

            case BINARY:
                return getBinary();

            case STRING:
                return getString();

            case SYMBOL:
                return getSymbol();

            case DESCRIBED:
                return getDescribedType();

            case ARRAY:
                return getJavaArray();

            case LIST:
                return getJavaList();

            case MAP:
                return getJavaMap();
        }
        return null;
    }

    @Override
    public Map<Object, Object> getJavaMap()
    {
        int count = (int) getMap();
        Map<Object,Object> map = new LinkedHashMap<Object, Object>(count);
        enter();
        for(int i = 0; i < count; i+=2)
        {
            Object key = next() != null ? getObject() : null;
            Object value = next() != null ? getObject() : null;
	    map.put(key, value);
        }
        exit();
        return map;
    }

    @Override
    public List<Object> getJavaList()
    {
        int count = (int) getList();
        List<Object> list = new ArrayList<Object>(count);
        enter();
        for(int i = 0; i < count; i++)
        {
            next();
            list.add(getObject());
        }
        exit();
        return list;
    }

    @Override
    public Object[] getJavaArray()
    {
        int count = (int)getArray();
        boolean described = isArrayDescribed();
        DataType t = getArrayType();

        Object descriptor;
        enter();
        Object[] arr;
	next();
        if(described)
        {
            descriptor = getObject();
            next();
            arr = new DescribedType[count];
        }
        else
        {
            descriptor = null;
            Class clazz = CLASSMAP.get(t);
            arr = (Object[]) Array.newInstance(clazz, count);
        }


        for(int i = 0; i < count; i++)
        {
            arr[i] = described ? new DataDescribedType(descriptor,getObject()) : getObject();
            next();
        }
        exit();
        return arr;
    }

    @Override
    public DescribedType getDescribedType()
    {
        enter();
	next();
        final Object descriptor = getObject();
	next();
        final Object described = getObject();
        exit();
        return new DataDescribedType(descriptor, described);
    }

    @Override
    @ProtonCEquivalent("pn_data_copy")
    public void copy(final Data src)
    {
        Proton.pn_data_copy(_impl, ((JNIData) src)._impl);
    }

    @Override
    @ProtonCEquivalent("pn_data_append")
    public void append(final Data src)
    {
        Proton.pn_data_append(_impl, ((JNIData) src)._impl);
    }

    @Override
    @ProtonCEquivalent("pn_data_appendn")
    public void appendn(final Data src, final int limit)
    {
        Proton.pn_data_appendn(_impl, ((JNIData) src)._impl, limit);
    }

    @Override
    @ProtonCEquivalent("pn_data_narrow")
    public void narrow()
    {
        Proton.pn_data_narrow(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_data_widen")
    public void widen()
    {
        Proton.pn_data_widen(_impl);
    }

    @ProtonCEquivalent("pn_data_format")
    public String toString()
    {
        int size = 16;

        byte[] data;
        while(true)
        {
            data = new byte[size];
            ByteBuffer buf = ByteBuffer.wrap(data);
            if(Proton.pn_data_format(_impl,buf) != Proton.PN_OVERFLOW)
            {
                break;
            };
            size = size*2;
        }
        return new String(data);
    }

    private static class DataDescribedType implements DescribedType
    {
        private final Object _descriptor;
        private final Object _described;

        public DataDescribedType(final Object descriptor, final Object described)
        {
            _descriptor = descriptor;
            _described = described;
        }

        @Override
        public Object getDescriptor()
        {
            return _descriptor;
        }

        @Override
        public Object getDescribed()
        {
            return _described;
        }
    }
}
