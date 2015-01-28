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
 * Example
 *
 */

public class Example
{

    public static final void main(String[] args) {
        byte[] bytes = new byte[1024];

        ByteArrayEncoder enc = new ByteArrayEncoder();
        enc.init(bytes, 0, 1024);
        enc.putInt(3);
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
        /**/
        ByteArrayDecoder dec = new ByteArrayDecoder();
        dec.init(bytes, 0, size);
        POJOBuilder pb = new POJOBuilder();
        dec.decode(pb);
        System.out.println(pb.build());
        /**/
        List l = (List) pb.build();
        System.out.println(Arrays.toString((Object[]) l.get(4)));
        System.out.println(Arrays.toString((Object[]) l.get(5)));
    }

}
