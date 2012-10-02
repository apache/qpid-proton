
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
import java.util.Map;
import java.util.List;
import java.util.AbstractList;


import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.type.*;


public class Error
      implements DescribedType 
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x000000000000001dL), Symbol.valueOf("amqp:error:list"), 
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x000000000000001dL);
    private final ErrorWrapper _wrapper = new ErrorWrapper();
    
    private Symbol _condition;
    private String _description;
    private Map _info;

    public Symbol getCondition()
    {
        return _condition;
    }

    public void setCondition(Symbol condition)
    {
        if( condition == null )
        {
            throw new NullPointerException("the condition field is mandatory");
        }

        _condition = condition;
    }

    public String getDescription()
    {
        return _description;
    }

    public void setDescription(String description)
    {
        _description = description;
    }

    public Map getInfo()
    {
        return _info;
    }

    public void setInfo(Map info)
    {
        _info = info;
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
                return _condition;
            case 1:
                return _description;
            case 2:
                return _info;            
        }

        throw new IllegalStateException("Unknown index " + index);

    }

    public int size()
    {
        return _info != null 
                  ? 3 
                  : _description != null 
                  ? 2 
                  : 1;        

    }


    public final class ErrorWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return Error.this.get(index);
        }

        @Override
        public int size()
        {
            return Error.this.size();
        }
    }

    private static class ErrorConstructor implements DescribedTypeConstructor<Error>
    {
        public Error newInstance(Object described)
        {
            List l = (List) described;

            Error o = new Error();

            if(l.size() <= 0)
            {
                throw new DecodeException("The condition field cannot be omitted");
            }

            switch(3 - l.size())
            {

                case 0:
                    o.setInfo( (Map) l.get( 2 ) );
                case 1:
                    o.setDescription( (String) l.get( 1 ) );
                case 2:
                    o.setCondition( (Symbol) l.get( 0 ) );
            }


            return o;
        }

        public Class<Error> getTypeClass()
        {
            return Error.class;
        }
    }


    public static void register(Decoder decoder)
    {
        ErrorConstructor constructor = new ErrorConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }

    @Override
    public String toString()
    {
        return "Error{" +
               "_condition=" + _condition +
               ", _description='" + _description + '\'' +
               ", _info=" + _info +
               '}';
    }
}
  