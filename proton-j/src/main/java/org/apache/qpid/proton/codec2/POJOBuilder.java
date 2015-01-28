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

import java.util.*;

/**
 * POJOBuilder
 *
 */

public class POJOBuilder implements DataHandler
{

    public static class Described {

        private Object descriptor;
        private Object value;

        public Described(Object descriptor, Object value) {
            this.descriptor = descriptor;
            this.value = value;
        }

        public Object getDescriptor() {
            return descriptor;
        }

        public Object getValue() {
            return value;
        }

        public String toString() {
            return String.format("Described(%s, %s)", descriptor, value);
        }

    }

    private interface Builder {
        void add(Object o);
        Object build();
    }

    private class DescribedBuilder implements Builder {

        private Object descriptor;
        private Described described;
        private boolean first = true;

        public DescribedBuilder() {}

        public void add(Object o) {
            if (first) {
                descriptor = o;
                first = false;
            } else {
                described = new Described(descriptor, o);
                end();
            }
        }

        public Described build() {
            return described;
        }

    }

    private class ListBuilder implements Builder {

        private List list;

        public ListBuilder(int count) {
            list = new ArrayList(count);
        }

        public void add(Object o) {
            list.add(o);
        }

        public List build() {
            return list;
        }

    }

    private class MapBuilder implements Builder {

        private Map map;
        private Object key;
        private int count;

        public MapBuilder(int count) {
            map = new HashMap(count/2);
            this.count = 0;
        }

        public void add(Object o) {
            if ((count % 2) == 0) {
                key = o;
            } else {
                map.put(key, o);
            }
            count++;
        }

        public Map build() {
            return map;
        }

    }

    private class ArrayBuilder implements Builder {

        private Object[] array;
        private int index;
        private boolean described;
        private Object descriptor;

        public ArrayBuilder(int count) {
            // XXX: should really instantiate the proper type here
            array = new Object[count];
            index = 0;
            described = false;
        }

        public void add(Object o) {
            if (described) {
                if (index == 0) {
                    descriptor = o;
                } else {
                    array[index - 1] = new Described(descriptor, o);
                }
            } else {
                array[index] = o;
            }
            index++;
        }

        public Object[] build() {
            return array;
        }

    }

    private List<Builder> stack = new ArrayList();
    private Builder builder;

    public POJOBuilder() {
        clear();
    }

    public void clear() {
        stack.clear();
        builder = new ListBuilder(1);
    }

    public Object build() {
        return builder.build();
    }

    private void pop() {
        builder = stack.remove(stack.size() - 1);
    }

    private void push() {
        stack.add(builder);
    }

    private void end() {
        Object o = builder.build();
        pop();
        builder.add(o);
    }

    @Override
    public void onList(Decoder decoder) {
        push();
        builder = new ListBuilder(decoder.getSize());
    }

    @Override
    public void onListEnd(Decoder decoder) {
        end();
    }

    @Override
    public void onMap(Decoder decoder) {
        push();
        builder = new MapBuilder(decoder.getSize());
    }

    @Override
    public void onMapEnd(Decoder decoder) {
        end();
    }

    @Override
    public void onArray(Decoder decoder) {
        push();
        builder = new ArrayBuilder(decoder.getSize());
    }

    @Override
    public void onArrayEnd(Decoder decoder) {
        end();
    }

    @Override
    public void onDescriptor(Decoder decoder) {
        if (builder instanceof ArrayBuilder) {
            ArrayBuilder ab = (ArrayBuilder) builder;
            ab.described = true;
        } else {
            push();
            builder = new DescribedBuilder();
        }
    }

    @Override
    public void onNull(Decoder decoder) {
        builder.add(null);
    }

    @Override
    public void onBoolean(Decoder decoder) {
        // TODO
    }

    @Override
    public void onByte(Decoder decoder) {
        // TODO
    }

    @Override
    public void onShort(Decoder decoder) {
        // TODO
    }

    @Override
    public void onInt(Decoder decoder) {
        builder.add(decoder.getInt());
    }

    @Override
    public void onLong(Decoder decoder) {
        // TODO
    }

    @Override
    public void onUbyte(Decoder decoder) {
        // TODO
    }

    @Override
    public void onUshort(Decoder decoder) {
        // TODO
    }

    @Override
    public void onUint(Decoder decoder) {
        // TODO
    }

    @Override
    public void onUlong(Decoder decoder) {
        // TODO
    }

    public void onFloat(Decoder decoder) {
        builder.add(decoder.getFloat());
    }

    @Override
    public void onDouble(Decoder decoder) {
        builder.add(decoder.getDouble());
    }

    @Override
    public void onChar(Decoder decoder) {
        builder.add((char) decoder.getIntBits());
    }

    @Override
    public void onTimestamp(Decoder decoder) {
        // TODO
        builder.add(decoder.getLong());
    }

    @Override
    public void onUUID(Decoder decoder) {
        builder.add(new UUID(decoder.getHiBits(), decoder.getLoBits()));
    }

    @Override
    public void onDecimal32(Decoder decoder) {
        // TODO
    }

    @Override
    public void onDecimal64(Decoder decoder) {
        // TODO
    }

    @Override
    public void onDecimal128(Decoder decoder) {
        // TODO
    }

    @Override
    public void onString(Decoder decoder) {
        builder.add(decoder.getString());
    }

    @Override
    public void onSymbol(Decoder decoder) {
        builder.add(decoder.getString());
    }

    @Override
    public void onBinary(Decoder decoder) {
        // TODO
        //builder.add(decoder.getBytes());
    }

}
