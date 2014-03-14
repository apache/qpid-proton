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

package org.apache.qpid.proton.message.impl;

import java.math.BigDecimal;
import java.nio.CharBuffer;
import java.text.DecimalFormat;
import java.util.*;
import org.apache.qpid.proton.amqp.DescribedType;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.transport.Flow;
import org.apache.qpid.proton.message.*;


class AMQPMessageFormat
{

    private static final char START_LIST = '[';
    private static final char DESCRIPTOR_CHAR = '@';
    private static final char SYMBOL_START = ':';
    private static final char QUOTE_CHAR = '"';
    private static final char START_MAP = '{';
    private static final char END_LIST = ']';
    private static final char END_MAP = '}';
    private static final char[] HEX = "0123456789abcdef".toCharArray();


    public String encode(Object o)
    {
        if(o instanceof Boolean)
        {
            return encodeBoolean((Boolean)o);
        }
        if(o instanceof byte[])
        {
            return encodeBinary((byte[]) o);
        }
        if(o instanceof Number)
        {
            return encodeNumber((Number) o);
        }
        if(o instanceof String)
        {
            return encodeString((String) o);
        }
        if(o instanceof Symbol)
        {
            return encodeSymbol((Symbol) o);
        }
        if(o instanceof List)
        {
            return encodeList((List)o);
        }
        if(o instanceof Map)
        {
            return encodeMap((Map) o);
        }
        if(o instanceof DescribedType)
        {
            return encodeDescribedType((DescribedType)o);
        }
        if(o == null)
        {
            return "null";
        }
        return null;
    }

    private String encodeBinary(byte[] o)
    {
        StringBuilder b = new StringBuilder();
        b.append("b\"");

        for(byte x : o)
        {
            if(x >= 32 && x < 127 && x != '"' && x != '\\')
            {
                b.append((char)x);
            }
            else
            {
                b.append("\\x");
                b.append(HEX[(x>>4) & 0x0f ]);
                b.append(HEX[(x) & 0x0f ]);
            }
        }

        b.append('"');
        return b.toString();
    }

    private String encodeNumber(Number o)
    {
        if(o instanceof Float || o instanceof Double || o instanceof BigDecimal)
        {
            DecimalFormat df = new DecimalFormat("############.#######");
            return df.format(o);
        }
        else
        {
            Formatter f = new Formatter();
            return f.format("%d", o.longValue()).toString();
        }
    }

    private String encodeBoolean(boolean o)
    {
        return o ? "true" : "false";
    }

    private String encodeDescribedType(DescribedType o)
    {
        StringBuilder b = new StringBuilder();
        b.append(DESCRIPTOR_CHAR);
        b.append(encode(o.getDescriptor()));
        b.append(' ');
        b.append(encode(o.getDescribed()));
        return b.toString();
    }

    private String encodeMap(Map<Object,Object> o)
    {
        StringBuilder b = new StringBuilder();
        b.append(START_MAP);
        boolean first = true;
        for(Map.Entry<Object,Object> e : o.entrySet())
        {
            if(first)
            {
                first = false;
            }
            else
            {
                b.append(',');
                b.append(' ');
            }

            b.append(encode(e.getKey()));
            b.append('=');
            b.append(encode(e.getValue()));

        }
        b.append(END_MAP);
        return b.toString();
    }

    private String encodeList(List o)
    {
        StringBuilder b = new StringBuilder();
        b.append(START_LIST);
        boolean first = true;
        for(Object e : o)
        {
            if(first)
            {
                first = false;
            }
            else
            {
                b.append(',');
                b.append(' ');
            }

            b.append(encode(e));

        }
        b.append(END_LIST);
        return b.toString();
    }

    private String encodeSymbol(Symbol o)
    {
        StringBuilder b = new StringBuilder(":");
        String sym = o.toString();
        if(needsQuoting(sym))
        {
            b.append(encodeString(sym));
        }
        else
        {
            b.append(sym);
        }
        return b.toString();
    }

    private boolean needsQuoting(String sym)
    {
        for(char c : sym.toCharArray())
        {
            if((c < '0' || c > '9') && (c <'a' || c>'z') && (c<'A' || c>'Z') && c != '_')
            {
                return true;
            }
        }
        return false;
    }

    private String encodeString(String o)
    {
        StringBuilder b = new StringBuilder();
        b.append("\"");
        for(char c : ((String) o).toCharArray())
        {
                if(c == '\\')
                {
                    b.append("\\\\");
                }
                else if( c == '\n')
                {
                    b.append("\\n");
                }
                else if( c == '\b')
                {
                    b.append("\\b");
                }
                else if( c == '\r')
                {
                    b.append("\\r");
                }
                else if( c == '\t')
                {
                    b.append("\\t");
                }
                else if( c == '\"')
                {
                    b.append("\\\"");
                }
                else if(c < 32 || c > 127)
                {
                    Formatter fmt = new Formatter();
                    b.append("\\u");
                    b.append(fmt.format("%04x",(int)c));
                }
                else
                {
                    b.append(c);
                }

        }
        b.append("\"");
        return b.toString();
    }

    public static void main(String[] args)
    {
        Map m = new LinkedHashMap();
        final Object x = Arrays.asList((Object) Symbol.valueOf("hel lo"), "hi", Symbol.valueOf("x"));
        m.put(Symbol.valueOf("ddd"), x);
        m.put("hello", "world");
        final Flow flow = new Flow();
        flow.setDrain(true);
        m.put(flow, "wibble");
        final AMQPMessageFormat f = new AMQPMessageFormat();
        System.err.println(f.encode(m));

        byte[] data = {0, 23, 45, 98, (byte) 255, 32, 78, 12, 126, 127, (byte) 128, 66,67,68};
        System.err.println(f.encodeBinary(data));
        byte[] data2 = (byte[]) f.format(f.encode(data));

        System.out.println(Arrays.equals(data,data2));
    }

    public Object format(CharSequence s)
    {
        return readValue(CharBuffer.wrap(s));
    }

    private void skipWhitespace(CharBuffer s)
    {
        while(s.hasRemaining())
        {
            char c = s.get();
            if(!Character.isWhitespace(c))
            {
                s.position(s.position()-1);
                break;
            }
        }


    }

    private Object readValue(CharBuffer s)
    {
        skipWhitespace(s);
        if(!s.hasRemaining())
        {
            throw new MessageFormatException("Expecting a value, but only whitespace found", s);
        }

        char c = s.get(s.position());

        if(Character.isDigit(c) || c == '-' )
        {
            return readNumber(s);
        }
        else if(c== SYMBOL_START)
        {
            return readSymbol(s);
        }
        else if(c== START_MAP)
        {
            return readMap(s);
        }
        else if(c== START_LIST)
        {
            return readList(s);
        }
        else if(c=='b')
        {
            return readBinary(s);
        }
        else if(c== DESCRIPTOR_CHAR)
        {
            return readDescribedType(s);
        }
        else if(c == 't')
        {
            expect(s, "true");
            return Boolean.TRUE;
        }
        else if(c == 'f')
        {
            expect(s, "false");
            return Boolean.FALSE;
        }
        else if(c == 'n')
        {
            expect(s, "null");
            return null;
        }
        else if(c == '\"')
        {
            return readString(s);
        }

        throw new MessageFormatException("Cannot parse message string", s);
    }


    private Object readDescribedType(CharBuffer s)
    {
        expect(s, DESCRIPTOR_CHAR);
        final Object descriptor = readValue(s);
        final Object described = readValue(s);
        return new DescribedType() {

            public Object getDescriptor()
            {
                return descriptor;
            }

            public Object getDescribed()
            {
                return described;
            }

            @Override
            public int hashCode()
            {
                return super.hashCode();
            }

            @Override
            public boolean equals(Object obj)
            {

                return obj instanceof DescribedType
                       && descriptor == null ? ((DescribedType) obj).getDescriptor() == null
                                             : descriptor.equals(((DescribedType) obj).getDescriptor())
                       && described == null ?  ((DescribedType) obj).getDescribed() == null
                                            : described.equals(((DescribedType) obj).getDescribed());

            }

            @Override
            public String toString()
            {
                return ""+DESCRIPTOR_CHAR + descriptor + " " + described;
            }
        };

    }

    private Object readBinary(CharBuffer s)
    {
        byte[] bytes = new byte[s.remaining()];
        int length = 0;
        expect(s, "b\"");
        char c;
        while(s.hasRemaining())
        {
            c = s.get();

            if(c == '\\')
            {
                expect(s, 'x');
                if(s.remaining()<2)
                {
                    throw new MessageFormatException("binary string escaped numeric value not correctly formatted", s);
                }
                byte val;
                c = s.get();
                if(c >= 'a' && c <= 'f')
                {
                    val = (byte) (16 * (10 + (c - 'a')));
                }
                else if(c >= 'A' && c <= 'F')
                {
                    val = (byte) (16 * (10 + (c - 'A')));
                }
                else if(c >= '0' && c <= '9')
                {
                    val = (byte) (16 * (c - '0'));
                }
                else
                {
                    throw new MessageFormatException("invalid value", s);
                }
                c = s.get();

                if(c >= 'a' && c <= 'f')
                {
                    val += (byte) (10 + (c - 'a'));
                }
                else if(c >= 'A' && c <= 'F')
                {
                    val += (byte) (10 + (c - 'A'));
                }
                else if(c >= '0' && c <= '9')
                {
                    val += (byte) (c - '0');
                }
                else
                {
                    throw new MessageFormatException("invalid value", s);
                }
                bytes[length++] = val;

            }
            else if(c == QUOTE_CHAR)
            {
                byte[] rval = new byte[length];
                System.arraycopy(bytes,0,rval,0,length);
                return rval;
            }
            else if(c < 256)
            {
               bytes[length++] = (byte) c;
            }
        }
        throw new MessageFormatException("unterminated binary string", s);
    }

    private Object readList(CharBuffer s)
    {
        List<Object> list = new ArrayList<Object>();
        expect(s, START_LIST);

        char c;
        while(s.hasRemaining())
        {
            skipWhitespace(s);
            c = s.get(s.position());
            if(c == END_LIST)
            {
                c = s.get();
                return list;
            }
            if(!list.isEmpty())
            {
                expect(s, ',');
            }

            list.add(readValue(s));
        }
        throw new MessageFormatException("unterminated list", s);
    }

    private Object readMap(CharBuffer s)
    {
        Map<Object, Object> map = new LinkedHashMap<Object, Object>();

        expect(s, START_MAP);
        char c;


        while(s.hasRemaining())
        {
            skipWhitespace(s);
            c = s.get(s.position());
            if(c == END_MAP)
            {
                c = s.get();
                return map;
            }
            if(!map.isEmpty())
            {
                expect(s, ',');
            }
            Object key = readValue(s);
            skipWhitespace(s);
            expect(s, '=');
            Object value = readValue(s);
            map.put(key,value);
        }
        throw new MessageFormatException("unterminated map", s);

    }

    private void expect(CharBuffer s, String t)
    {
        expect(s, t.toCharArray());
    }

    private void expect(CharBuffer s, char... expect)
    {
        for(char e : expect)
        {
            char c = s.get();
            if(c != e)
            {
                throw new IllegalArgumentException("expecting "+expect+" character" + s.toString());
            }
        }
    }

    private Object readSymbol(CharBuffer s)
    {
        char c = s.get();
        if(c != SYMBOL_START)
        {
            throw new IllegalArgumentException("expecting @ as first character" + s.toString());
        }

        c = s.get(s.position());
        if(c == QUOTE_CHAR)
        {
            return readQuotedSymbol(s);
        }
        else
        {
            return readUnquotedSymbol(s);
        }
    }

    private Object readUnquotedSymbol(CharBuffer s)
    {
        StringBuilder b = new StringBuilder();
        while(s.hasRemaining())
        {
            char c = s.get(s.position());
            if(Character.isWhitespace(c) || c == '=' || c == ',')
            {
                break;
            }
            if(c > 255)
            {
                throw new MessageFormatException("Illegal character " + c, s);
            }
            s.get();
            b.append(c);

        }
        return Symbol.valueOf(b.toString());
    }

    private Object readQuotedSymbol(CharBuffer s)
    {
        String str = readString(s);
        return Symbol.valueOf(str);
    }

    private String readString(CharBuffer s)
    {
        expect(s, '\"');
        StringBuilder b = new StringBuilder();
        while(s.hasRemaining())
        {
            char c = s.get();
            if(c == QUOTE_CHAR)
            {
                return b.toString();
            }
            else if(c == '\\')
            {
                if(!s.hasRemaining())
                {
                    throw new MessageFormatException("Unterminated escape",s);
                }
                c = s.get();
                switch(c)
                {

                    case 'b':
                        b.append('\b');
                        break;
                    case 'f':
                        b.append('\f');
                        break;
                    case 'n':
                        b.append('\n');
                        break;
                    case 'r':
                        b.append('\r');
                        break;
                    case 't':
                        b.append('\t');
                        break;
                    case '\\':
                        b.append('\\');
                        break;
                    case '\"':
                        b.append('\"');
                        break;
                    case 'u':
                        if(s.remaining()<4)
                        {
                            throw new MessageFormatException("Incorrect format for unicode character",s);
                        }
                        char u;
                        String num = new String(new char[] {s.get(),s.get(),s.get(),s.get()});
                        u = (char) Integer.parseInt(num, 16);

                        b.append(u);
                        break;
                    default:
                        b.append(c);
                }
            }
            else
            {
                b.append(c);
            }
        }
        throw new MessageFormatException("unterminated map", s);

    }

    private Object readNumber(CharBuffer s)
    {
        StringBuilder b = new StringBuilder();
        while(s.hasRemaining())
        {
            char c = s.get(s.position());
            if(Character.isWhitespace(c) || c == END_LIST || c == END_MAP || c == ',' || c == '=' )
            {
                break;
            }
            else
            {
                c = s.get();
            }
            b.append(c);
        }
        Number num;
        String numberString = b.toString();
        if(numberString.contains("."))
        {
            num = Double.parseDouble(numberString);
        }
        else
        {
            num = Long.parseLong(numberString);
            if(num.longValue() >= Integer.MIN_VALUE && num.longValue() <= Integer.MAX_VALUE)
            {
                num  = num.intValue();
            }
        }

        return num;
    }
}
