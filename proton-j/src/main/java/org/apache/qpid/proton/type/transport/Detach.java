
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


package org.apache.qpid.proton.type.transport;
import java.util.List;
import java.util.AbstractList;


import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.type.*;


public class Detach
      implements DescribedType , FrameBody
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000016L), Symbol.valueOf("amqp:detach:list"), 
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000016L);
    private final DetachWrapper _wrapper = new DetachWrapper();
    
    private UnsignedInteger _handle;
    private boolean _closed;
    private Error _error;

    public UnsignedInteger getHandle()
    {
        return _handle;
    }

    public void setHandle(UnsignedInteger handle)
    {
        if( handle == null )
        {
            throw new NullPointerException("the handle field is mandatory");
        }

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

    public Error getError()
    {
        return _error;
    }

    public void setError(Error error)
    {
        _error = error;
    }
    
    public Object getDescriptor()
    {
        return DESCRIPTOR;
    }

    public Object getDescribed()
    {
        return _wrapper;
    }
    
    public Object get(final int index)
    {

        switch(index)
        {
            case 0:
                return _handle;
            case 1:
                return _closed;
            case 2:
                return _error;            
        }

        throw new IllegalStateException("Unknown index " + index);

    }

    public int size()
    {
        return _error != null 
                  ? 3 
                  : (_closed != false)
                  ? 2 
                  : 1;        

    }


    public final class DetachWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return Detach.this.get(index);
        }

        @Override
        public int size()
        {
            return Detach.this.size();
        }
    }

    private static class DetachConstructor implements DescribedTypeConstructor<Detach>
    {
        public Detach newInstance(Object described)
        {
            List l = (List) described;

            Detach o = new Detach();

            if(l.size() <= 0)
            {
                throw new DecodeException("The handle field cannot be omitted");
            }

            switch(3 - l.size())
            {

                case 0:
                    o.setError( (Error) l.get( 2 ) );
                case 1:
                    Boolean closed = (Boolean) l.get(1);
                    o.setClosed(closed == null ? false : closed);
                case 2:
                    o.setHandle( (UnsignedInteger) l.get( 0 ) );
            }


            return o;
        }

        public Class<Detach> getTypeClass()
        {
            return Detach.class;
        }
    }

    public <E> void invoke(FrameBodyHandler<E> handler, Binary payload, E context)
    {
        handler.handleDetach(this, payload, context);
    }


    public static void register(Decoder decoder)
    {
        DetachConstructor constructor = new DetachConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }

    @Override
    public String toString()
    {
        return "Detach{" +
               "handle=" + _handle +
               ", closed=" + _closed +
               ", error=" + _error +
               '}';
    }
}
  