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

import java.io.FileOutputStream;
import java.io.FileWriter;

import org.apache.qpid.proton.transport2.Attach;
import org.apache.qpid.proton.transport2.Source;
import org.apache.qpid.proton.transport2.Target;
import org.apache.qpid.proton.transport2.Transfer;

/**
 * Example
 *
 */

public class Example
{

    public static final void main(String[] args) throws Exception {
        byte[] bytes = new byte[1024];

        ByteArrayEncoder enc = new ByteArrayEncoder();
        enc.init(bytes, 0, 1024);
        
        method5(enc);
        
        int size = enc.getPosition();
        
        toFile(bytes, 0, size);
        
        /*enc.putInt(3);
        enc.putDescriptor();
        enc.putSymbol("url");
        enc.putString("http://example.org");
        enc.putList();
        enc.putInt(1);
        enc.putInt(2);
        enc.putInt(3);
        enc.putFloat((float) 2.0);
        enc.putDouble(4.0);
        enc.end();
        enc.putString("this is a test");
        enc.putArray(Type.INT);
        enc.putInt(1);
        enc.putInt(2);
        enc.putInt(3);
        enc.end();
        enc.putArray(Type.STRING);
        enc.putDescriptor();
        enc.putSymbol("url");
        enc.putString("http://one");
        enc.putString("http://two");
        enc.putString("http://three");
        enc.end();
        enc.putMap();
        enc.putString("key");
        enc.putString("value");
        enc.putString("pi");
        enc.putDouble(3.14159265359);
        enc.end();

        int size = enc.getPosition();

        //System.out.write(bytes, 0, size);
        
        ByteArrayDecoder dec = new ByteArrayDecoder();
        dec.init(bytes, 0, size);
        POJOBuilder pb = new POJOBuilder();
        dec.decode(pb);
        System.out.println(pb.build());
        
        List l = (List) pb.build();
        System.out.println("First item : " + l.get(0));
        System.out.println("Second item : " + l.get(1));
        System.out.println(Arrays.toString((Object[]) l.get(4)));
        System.out.println(Arrays.toString((Object[]) l.get(5))); 
        **/
        
        ByteArrayDecoder dec = new ByteArrayDecoder();
        dec.init(bytes, 0, size);
        POJOBuilder pb = new POJOBuilder();
        dec.decode(pb);
        System.out.println(pb.build());
    }
    
    static void toFileHex(byte[] bytes, int offset, int len) throws Exception
    {
        FileWriter fout = new FileWriter("/home/rajith/data/NestedData");
        fout.write(bytesToHex(bytes, 0, len));
        fout.flush();
        fout.close();
    }

    static void toFile(byte[] bytes, int offset, int len) throws Exception
    {
        FileOutputStream fout = new FileOutputStream("/home/rajith/data/NestedData");
        fout.write(bytes, 0, len);
        fout.flush();
        fout.close();
    }

    final protected static char[] hexArray = "0123456789ABCDEF".toCharArray();
    public static String bytesToHex(byte[] bytes, int offset, int len) {
        char[] hexChars = new char[len * 2];
        for ( int j = offset; j < len; j++ ) {
            int v = bytes[j] & 0xFF;
            hexChars[j * 2] = hexArray[v >>> 4];
            hexChars[j * 2 + 1] = hexArray[v & 0x0F];
        }
        return new String(hexChars);
    }
    
    public static void method1(Encoder enc)
    {
        enc.putMap();
        enc.putString("List");
        enc.putList();
        enc.putInt(1);
        enc.putInt(2);
        enc.putInt(3);
        enc.end();
        enc.putString("Name");
        enc.putString("Rajith");        
        enc.end();
    }

    
    public static void method2(Encoder enc)
    {
        enc.putMap();
        enc.putString("List");
        enc.putList();
        enc.putInt(1);
        enc.putInt(2);
        enc.putInt(3);
        enc.end();
        enc.putString("Name");
        enc.putString("Rajith");
        enc.putString("List2");
        enc.putList();
        enc.putInt(1);
        enc.putInt(2);
        enc.putInt(3);
        enc.end();
        enc.end();
    }
    
    public static void method3(Encoder enc)
    {
        enc.putMap();
        enc.putString("List");
        enc.putList();
        enc.putInt(1);
        enc.putInt(2);
        enc.putInt(3);
        enc.end();
        enc.putString("Name");
        enc.putString("Rajith");
        enc.putString("Inner Map");
        enc.putMap();
        enc.putString("InnerList");
        enc.putString("FakeListValue");
        /*enc.putList();
        enc.putInt(1);
        enc.putInt(2);
        enc.putInt(3);
        enc.end();*/
        enc.end();
        enc.end();
    }

    public static void method4(Encoder enc)
    {
        Attach a = new Attach();
        a.setName("Hello");
        Source s = new Source();
        s.setAddress("hello");
        Target t = new Target();
        a.setSource(s);
        a.setTarget(t);
        a.encode(enc);
    }
    
    public static void method5(Encoder enc)
    {
        Transfer t = new Transfer();
        t.setDeliveryTag("hello".getBytes());
        t.setHandle(1);
        t.encode(enc);
    }
}


