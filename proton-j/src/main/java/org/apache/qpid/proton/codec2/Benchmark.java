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

import java.util.UUID;
import org.apache.qpid.proton.codec.*;

import java.nio.*;
import java.util.*;

/**
 * Benchmark
 *
 */

public class Benchmark
{

    public static final void main(String[] args) {
        int loop = 10*1024*1024;
        if (args.length > 0) {
            loop = Integer.parseInt(args[0]);
        }

        String test = "all";
        if (args.length > 1) {
            test = args[1];
        }

        boolean runNew = test.equals("all") || test.equals("new");
        boolean runExisting = test.equals("all") || test.equals("existing");

        byte[] bytes = new byte[1024];
        ByteBuffer buf = ByteBuffer.wrap(bytes);

        long start, end;

        if (runNew) {
            start = System.currentTimeMillis();
            //int size = newEncode(bytes, loop);
            end = System.currentTimeMillis();
            time("new encode", start, end);

            start = System.currentTimeMillis();
            //newDecode(bytes, size, loop);
            end = System.currentTimeMillis();
            time("new decode", start, end);
            
            start = System.currentTimeMillis();
            int size = newEncodeString(bytes, loop);
            end = System.currentTimeMillis();
            time("new encode string", start, end);

            start = System.currentTimeMillis();
            newDecodeString(bytes, size, loop);
            end = System.currentTimeMillis();
            time("new decode string", start, end);
        }

        if (runExisting) {
            start = System.currentTimeMillis();
            //DecoderImpl dec = existingEncode(buf, loop);
            end = System.currentTimeMillis();
            time("existing encode", start, end);

            buf.flip();

            start = System.currentTimeMillis();
            //existingDecode(dec, buf, loop);
            end = System.currentTimeMillis();
            time("existing decode", start, end);
            
            buf.flip();
            
            start = System.currentTimeMillis();
            DecoderImpl dec2 = existingEncodeString(buf, loop);
            end = System.currentTimeMillis();
            time("existing encode string", start, end);

            buf.flip();

            start = System.currentTimeMillis();
            existingDecodeString(dec2, buf, loop);
            end = System.currentTimeMillis();
            time("existing decode string", start, end);
        }
    }

    private static final void time(String message, long start, long end)
    {
        System.out.println(message + ": " + (end - start) + " millis");
    }

    private static final int newEncode(byte[] bytes, int loop)
    {
        ByteArrayEncoder enc = new ByteArrayEncoder();

        UUID uuid = UUID.randomUUID();
        long hi = uuid.getMostSignificantBits();
        long lo = uuid.getLeastSignificantBits();

        for (int i = 0; i < loop; i++) {
            enc.init(bytes, 0, bytes.length);
            enc.putList();
            for (int j = 0; j < 10; j++) {
                enc.putInt(i + j);
            }
            enc.end();
            enc.putUUID(hi, lo);
            enc.putMap();
            for (int j = 0; j < 10; j++) {
                enc.putString(String.valueOf(j)); 
                //enc.putInt(i);
                enc.putInt(i + j);
            }
            enc.end();
        }

        return enc.getPosition();
    }

    private static final void newDecode(byte[] bytes, int size, int loop)
    {
        DataHandler dh = new AbstractDataHandler() {

            public void onInt(org.apache.qpid.proton.codec2.Decoder d) {
                d.getIntBits();
            }

            public void onUUID(org.apache.qpid.proton.codec2.Decoder d) {
                d.getHiBits();
                d.getLoBits();
            }

            public void onString(org.apache.qpid.proton.codec2.Decoder d) {
                d.getString();
            }

        };

        ByteArrayDecoder dec = new ByteArrayDecoder();
        for (int i = 0; i < loop; i++) {
            dec.init(bytes, 0, size);
            dec.decode(dh);
        }
    }

    private static final DecoderImpl existingEncode(ByteBuffer buf, int loop)
    {
        WritableBuffer wbuf = new WritableBuffer.ByteBufferWrapper(buf);
        // XXX: why do I have to create a decoder in order to encode?
        DecoderImpl dec = new DecoderImpl();
        EncoderImpl enc = new EncoderImpl(dec);
        enc.setByteBuffer(wbuf);

        UUID uuid = UUID.randomUUID();
        long hi = uuid.getMostSignificantBits();
        long lo = uuid.getLeastSignificantBits();

        List list = new ArrayList();

        for (int i = 0; i < loop; i++) {
            buf.position(0);
            buf.limit(1024);
            list.clear();
            for (int j = 0; j < 10; j++) {
                list.add(i+j);
            }
            enc.writeList(list);
            enc.writeUUID(new UUID(hi, lo));
            Map<String, Integer> map = new HashMap<String, Integer>(); 
            for (int j = 0; j < 10; j++) {
                map.put(String.valueOf(j),i + j);
                //map.put(j,i + j);
            }
            enc.writeMap(map);
        }

        return dec;
    }

    private static final void existingDecode(DecoderImpl dec, ByteBuffer buf, int loop)
    {
        // XXX: for some reason if I create a new decoder it NPEs,
        // apparently I have to use the one that was passed to the
        // encoder?
        //ReadableBuffer rbuf = new ReadableBuffer.ByteBufferReader(buf);
        dec.setByteBuffer(buf);

        int pos = buf.position();

        for (int i = 0; i < loop; i++) {
            buf.position(pos);
            dec.readList();
            dec.readUUID();
            dec.readMap();
        }
    }

    
    private static final int newEncodeString(byte[] bytes, int loop)
    {
        ByteArrayEncoder enc = new ByteArrayEncoder();
        String str = "The brown fox jumped over the fence";
        
        for (int i = 0; i < loop*10000; i++) {
            enc.init(bytes, 0, bytes.length);
            enc.putString(str);
        }

        return enc.getPosition();
    }

    private static final void newDecodeString(byte[] bytes, int size, int loop)
    {
        DataHandler dh = new AbstractDataHandler() {

            public void onString(org.apache.qpid.proton.codec2.Decoder d) {
                d.getString();
            }
        };

        ByteArrayDecoder dec = new ByteArrayDecoder();
        for (int i = 0; i < loop; i++) {
            dec.init(bytes, 0, size);
            dec.decode(dh);
        }
    }

    private static final DecoderImpl existingEncodeString(ByteBuffer buf, int loop)
    {
        WritableBuffer wbuf = new WritableBuffer.ByteBufferWrapper(buf);
        // XXX: why do I have to create a decoder in order to encode?
        DecoderImpl dec = new DecoderImpl();
        EncoderImpl enc = new EncoderImpl(dec);
        enc.setByteBuffer(wbuf);

        String str = "The brown fox jumped over the fence";
        
        for (int i = 0; i < loop*10000; i++) {
            buf.position(0);
            buf.limit(1024);
            enc.writeString(str);
        }

        return dec;
    }

    private static final void existingDecodeString(DecoderImpl dec, ByteBuffer buf, int loop)
    {
        // XXX: for some reason if I create a new decoder it NPEs,
        // apparently I have to use the one that was passed to the
        // encoder?
        //ReadableBuffer rbuf = new ReadableBuffer.ByteBufferReader(buf);
        dec.setByteBuffer(buf);

        int pos = buf.position();

        for (int i = 0; i < loop; i++) {
            buf.position(pos);
            dec.readString();
        }
    }
}
