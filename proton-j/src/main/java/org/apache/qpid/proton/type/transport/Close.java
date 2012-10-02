
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


import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.type.*;


public class Close
      implements DescribedType , FrameBody
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000018L), Symbol.valueOf("amqp:close:list"), 
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000018L);
    private final CloseWrapper _wrapper = new CloseWrapper();
    
    private Error _error;

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
                return _error;            
        }

        throw new IllegalStateException("Unknown index " + index);

    }

    public int size()
    {
        return _error != null 
                  ? 1 
                  : 0;        

    }


    public final class CloseWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return Close.this.get(index);
        }

        @Override
        public int size()
        {
            return Close.this.size();
        }
    }

    private static class CloseConstructor implements DescribedTypeConstructor<Close>
    {
        public Close newInstance(Object described)
        {
            List l = (List) described;

            Close o = new Close();


            switch(1 - l.size())
            {

                case 0:
                    o.setError( (Error) l.get( 0 ) );
            }


            return o;
        }

        public Class<Close> getTypeClass()
        {
            return Close.class;
        }
    }

    public <E> void invoke(FrameBodyHandler<E> handler, Binary payload, E context)
    {
        handler.handleClose(this, payload, context);
    }


    public static void register(Decoder decoder)
    {
        CloseConstructor constructor = new CloseConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }

    @Override
    public String toString()
    {
        return "Close{" +
               "error=" + _error +
               '}';
    }
}
  