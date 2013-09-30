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
package org.apache.qpid.proton.engine.jni;

import java.nio.ByteBuffer;

import org.apache.qpid.proton.ProtonCEquivalent;
import org.apache.qpid.proton.engine.Sender;
import org.apache.qpid.proton.jni.Proton;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_link_t;

public class JNISender extends JNILink implements Sender
{
    JNISender(SWIGTYPE_p_pn_link_t link_t)
    {
        super(link_t);

    }

    @Override
    @ProtonCEquivalent("pn_link_offered")
    public void offer(int credits)
    {
        // TODO
    }

    @Override
    @ProtonCEquivalent("pn_link_send")
    public int send(byte[] bytes, int offset, int length)
    {
        return Proton.pn_link_send(getImpl(), ByteBuffer.wrap(bytes,offset,length));
    }

    @Override
    public void abort()
    {
        // TODO
    }

}
