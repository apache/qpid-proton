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
package org.apache.qpid.proton.codec2;

import java.util.List;
import java.util.Map;

/**
 * For the time being I'm using a separate helper class to hold the common
 * functionality related to codec. Later on a decision will be made on whether
 * to keep the code as it is, or move it a base class and use inheritance to
 * provide the functionality to concrete types. The current approach is
 * preferred as opposed to using inheritance.
 */
public class CodecHelper
{
    public static void encodeSymbolArray(Encoder encoder, String[] array)
    {
        encoder.putArray(Type.SYMBOL);
        for (String str : array)
        {
            encoder.putSymbol(str);
        }
        encoder.end();
    }

    public static void encodeMap(Encoder encoder, Map<Object, Object> map)
    {
        encoder.putMap();
        for (Object key : map.keySet())
        {
            encodeObject(encoder, key);
            encodeObject(encoder, map.get(key));
        }
        encoder.end();
    }

    public static void encodeList(Encoder encoder, List<Object> list)
    {
        encoder.putList();
        for (Object listEntry : (List<Object>) list)
        {
            encodeObject(encoder, listEntry);
        }
        encoder.end();
    }

    public static void encodeObject(final Encoder encoder, final Object o)
    {
        if (o.getClass().isPrimitive())
        {
            if (o instanceof Byte)
            {
                encoder.putByte((Byte) o);
            }
            else if (o instanceof Short)
            {
                encoder.putShort((Short) o);
            }
            else if (o instanceof Integer)
            {
                encoder.putInt((Integer) o);
            }
            else if (o instanceof Long)
            {
                encoder.putLong((Long) o);
            }
            else if (o instanceof Float)
            {
                encoder.putFloat((Float) o);
            }
            else if (o instanceof Double)
            {
                encoder.putDouble((Double) o);
            }
            else if (o instanceof Character)
            {
                encoder.putChar((Character) o);
            }
        }
        else if (o instanceof String)
        {
            encoder.putString((String) o);
        }
        else if (o.getClass().isArray())
        {
            Class<?> componentType = o.getClass().getComponentType();
            if (componentType.isPrimitive())
            {
                if (componentType == Boolean.TYPE)
                {
                    encoder.putArray(Type.BOOLEAN);
                    boolean[] array = (boolean[]) o;
                    for (boolean b : array)
                    {
                        encoder.putBoolean(b);
                    }
                    encoder.end();
                }
                else if (componentType == Byte.TYPE)
                {
                    encoder.putArray(Type.BYTE);
                    byte[] array = (byte[]) o;
                    for (byte b : array)
                    {
                        encoder.putByte(b);
                    }
                    encoder.end();
                }
                else if (componentType == Short.TYPE)
                {
                    encoder.putArray(Type.SHORT);
                    short[] array = (short[]) o;
                    for (short s : array)
                    {
                        encoder.putShort(s);
                    }
                    encoder.end();
                }
                else if (componentType == Integer.TYPE)
                {
                    encoder.putArray(Type.INT);
                    int[] array = (int[]) o;
                    for (int i : array)
                    {
                        encoder.putInt(i);
                    }
                    encoder.end();
                }
                else if (componentType == Long.TYPE)
                {
                    encoder.putArray(Type.LONG);
                    long[] array = (long[]) o;
                    for (long l : array)
                    {
                        encoder.putLong(l);
                    }
                    encoder.end();
                }
                else if (componentType == Float.TYPE)
                {
                    encoder.putArray(Type.FLOAT);
                    float[] array = (float[]) o;
                    for (float f : array)
                    {
                        encoder.putFloat(f);
                    }
                    encoder.end();
                }
                else if (componentType == Double.TYPE)
                {
                    encoder.putArray(Type.DOUBLE);
                    double[] array = (double[]) o;
                    for (double d : array)
                    {
                        encoder.putDouble(d);
                    }
                    encoder.end();
                }
                else if (componentType == Character.TYPE)
                {
                    encoder.putArray(Type.CHAR);
                    char[] array = (char[]) o;
                    for (char c : array)
                    {
                        encoder.putChar(c);
                    }
                    encoder.end();
                }
                else
                {
                    throw new IllegalArgumentException("Cannot write arrays of type " + componentType.getName());
                }
            }
            else
            {
                // handle arrays of strings, lists, maps ..etc
            }
        }
        else if (o instanceof List)
        {
            encoder.putList();
            for (Object listEntry : (List) o)
            {
                encodeObject(encoder, listEntry);
            }
            encoder.end();

        }
        else if (o instanceof Map)
        {
            encoder.putMap();
            Map map = (Map) o;
            for (Object k : map.keySet())
            {
                encodeObject(encoder, k);
                encodeObject(encoder, map.get(k));
            }
            encoder.end();
        }
        else if (o instanceof Described)
        {
            Described descType = (Described) o;
            encoder.putDescriptor();
            encodeObject(encoder, descType.getDescriptor());
            encodeObject(encoder, descType.getValue());
        }
        else
        {
            throw new IllegalArgumentException("Do not know how to write Objects of class " + o.getClass().getName());
        }
    }
}
