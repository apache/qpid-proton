
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


public class End
      implements DescribedType , FrameBody
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000017L), Symbol.valueOf("amqp:end:list"), 
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000017L);
    private final EndWrapper _wrapper = new EndWrapper();
    
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


    public final class EndWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return End.this.get(index);
        }

        @Override
        public int size()
        {
            return End.this.size();
        }
    }

    private static class EndConstructor implements DescribedTypeConstructor<End>
    {
        public End newInstance(Object described)
        {
            List l = (List) described;

            End o = new End();


            switch(1 - l.size())
            {

                case 0:
                    o.setError( (Error) l.get( 0 ) );
            }


            return o;
        }

        public Class<End> getTypeClass()
        {
            return End.class;
        }
    }

    public <E> void invoke(FrameBodyHandler<E> handler, Binary payload, E context)
    {
        handler.handleEnd(this, payload, context);
    }


    public static void register(Decoder decoder)
    {
        EndConstructor constructor = new EndConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }

    @Override
    public String toString()
    {
        return "End{" +
               "error=" + _error +
               '}';
    }
}
  