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

package org.apache.qpid.proton.transport;

import java.util.List;

import org.apache.qpid.proton.codec2.DecodeException;
import org.apache.qpid.proton.codec2.DescribedTypeFactory;
import org.apache.qpid.proton.codec2.Encodable;
import org.apache.qpid.proton.codec2.Encoder;

public final class Detach implements Encodable
{
    public final static long DESCRIPTOR_LONG = 0x0000000000000016L;

    public final static String DESCRIPTOR_STRING = "amqp:detach:list";

    private int _handle;

    private boolean _closed;

    private ErrorCondition _error;

    public int getHandle()
    {
        return _handle;
    }

    public void setHandle(int handle)
    {
        _handle = handle;
    }

    public boolean getClosed()
    {
        return _closed;
    }

    public void setClosed(boolean closed)
    {
        _closed = closed;
    }

    public ErrorCondition getError()
    {
        return _error;
    }

    public void setError(ErrorCondition error)
    {
        _error = error;
    }

    @Override
    public void encode(Encoder encoder)
    {
        encoder.putDescriptor();
        encoder.putUlong(DESCRIPTOR_LONG);
        encoder.putList();
        encoder.putUint(_handle);
        encoder.putBoolean(_closed);
        _error.encode(encoder);
        encoder.end();
    }

    public static final class Factory implements DescribedTypeFactory
    {
        @SuppressWarnings("unchecked")
        public Object create(Object in) throws DecodeException
        {
            List<Object> l = (List<Object>) in;
            Detach detach = new Detach();

            switch (3 - l.size())
            {

            case 0:
                detach.setError((ErrorCondition) l.get(2));
            case 1:
                detach.setClosed(l.get(1) == null ? false : (boolean) l.get(1));
            case 2:
                detach.setHandle((int) l.get(0));
            }

            return detach;
        }
    }

    @Override
    public String toString()
    {
        return "Detach{" + "handle=" + _handle + ", closed=" + _closed + ", error=" + _error + '}';
    }
}