
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


package org.apache.qpid.proton.codec.transport;

import java.util.AbstractList;
import java.util.List;
import java.util.Map;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.transport.ErrorCondition;
import org.apache.qpid.proton.codec.AbstractDescribedType;
import org.apache.qpid.proton.codec.DecodeException;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;


public final class ErrorConditionType extends AbstractDescribedType<ErrorCondition,List> implements DescribedTypeConstructor<ErrorCondition>
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x000000000000001dL), Symbol.valueOf("amqp:error:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x000000000000001dL);

    private ErrorConditionType(EncoderImpl encoder)
    {
        super(encoder);
    }

    public UnsignedLong getDescriptor()
    {
        return DESCRIPTOR;
    }

    @Override
    protected List wrap(ErrorCondition val)
    {
        return new ErrorConditionWrapper(val);
    }

    public static class ErrorConditionWrapper extends AbstractList
    {

        private ErrorCondition _errorCondition;

        public ErrorConditionWrapper(ErrorCondition errorCondition)
        {
            _errorCondition = errorCondition;
        }

        public Object get(final int index)
        {

            switch(index)
            {
                case 0:
                    return _errorCondition.getCondition();
                case 1:
                    return _errorCondition.getDescription();
                case 2:
                    return _errorCondition.getInfo();
            }

            throw new IllegalStateException("Unknown index " + index);

        }

        public int size()
        {
            return _errorCondition.getInfo() != null
                      ? 3
                      : _errorCondition.getDescription() != null
                      ? 2
                      : 1;

        }

    }

    public ErrorCondition newInstance(Object described)
    {
        List l = (List) described;

        ErrorCondition o = new ErrorCondition();

        if(l.isEmpty())
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

    public Class<ErrorCondition> getTypeClass()
    {
        return ErrorCondition.class;
    }



    public static void register(Decoder decoder, EncoderImpl encoder)
    {
        ErrorConditionType type = new ErrorConditionType(encoder);
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }

}
  