
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


package org.apache.qpid.proton.type.messaging;

import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.type.DescribedType;
import org.apache.qpid.proton.type.Symbol;
import org.apache.qpid.proton.type.UnsignedInteger;
import org.apache.qpid.proton.type.UnsignedLong;

import java.util.AbstractList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;


public class Target
      implements DescribedType , org.apache.qpid.proton.type.transport.Target
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000029L), Symbol.valueOf("amqp:target:list"), 
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000029L);
    private final TargetWrapper _wrapper = new TargetWrapper();
    
    private String _address;
    private UnsignedInteger _durable = UnsignedInteger.valueOf(0);
    private Symbol _expiryPolicy = Symbol.valueOf("session-end");
    private UnsignedInteger _timeout = UnsignedInteger.valueOf(0);
    private boolean _dynamic;
    private Map _dynamicNodeProperties;
    private Symbol[] _capabilities;

    public String getAddress()
    {
        return _address;
    }

    public void setAddress(String address)
    {
        _address = address;
    }

    public UnsignedInteger getDurable()
    {
        return _durable;
    }

    public void setDurable(UnsignedInteger durable)
    {
        _durable = durable;
    }

    public Symbol getExpiryPolicy()
    {
        return _expiryPolicy;
    }

    public void setExpiryPolicy(Symbol expiryPolicy)
    {
        _expiryPolicy = expiryPolicy;
    }

    public UnsignedInteger getTimeout()
    {
        return _timeout;
    }

    public void setTimeout(UnsignedInteger timeout)
    {
        _timeout = timeout;
    }

    public boolean getDynamic()
    {
        return _dynamic;
    }

    public void setDynamic(boolean dynamic)
    {
        _dynamic = dynamic;
    }

    public Map getDynamicNodeProperties()
    {
        return _dynamicNodeProperties;
    }

    public void setDynamicNodeProperties(Map dynamicNodeProperties)
    {
        _dynamicNodeProperties = dynamicNodeProperties;
    }

    public Symbol[] getCapabilities()
    {
        return _capabilities;
    }

    public void setCapabilities(Symbol... capabilities)
    {
        _capabilities = capabilities;
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
                return _address;
            case 1:
                return _durable;
            case 2:
                return _expiryPolicy;
            case 3:
                return _timeout;
            case 4:
                return _dynamic;
            case 5:
                return _dynamicNodeProperties;
            case 6:
                return _capabilities;            
        }

        throw new IllegalStateException("Unknown index " + index);

    }

    public int size()
    {
        return _capabilities != null 
                  ? 7 
                  : _dynamicNodeProperties != null 
                  ? 6 
                  : (_dynamic != false)
                  ? 5 
                  : (_timeout != null && !_timeout.equals(UnsignedInteger.ZERO))
                  ? 4 
                  : (_expiryPolicy != null && !_expiryPolicy.equals(TerminusExpiryPolicy.SESSION_END))
                  ? 3 
                  : (_durable != null && !_durable.equals(TerminusDurability.NONE))
                  ? 2 
                  : _address != null 
                  ? 1 
                  : 0;        

    }


    public final class TargetWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return Target.this.get(index);
        }

        @Override
        public int size()
        {
            return Target.this.size();
        }
    }

    private static class TargetConstructor implements DescribedTypeConstructor<Target>
    {
        public Target newInstance(Object described)
        {
            List l = (List) described;

            Target o = new Target();


            switch(7 - l.size())
            {

                case 0:
                    Object val0 = l.get( 6 );
                    if( val0 == null || val0.getClass().isArray() )
                    {
                        o.setCapabilities( (Symbol[]) val0 );
                    }
                    else
                    {
                        o.setCapabilities( (Symbol) val0 );
                    }
                case 1:
                    o.setDynamicNodeProperties( (Map) l.get( 5 ) );
                case 2:
                    Boolean dynamic = (Boolean) l.get(4);
                    o.setDynamic(dynamic == null ? false : dynamic);
                case 3:
                    UnsignedInteger timeout = (UnsignedInteger) l.get(3);
                    o.setTimeout(timeout == null ? UnsignedInteger.ZERO : timeout);
                case 4:
                    Symbol expiryPolicy = (Symbol) l.get(2);
                    o.setExpiryPolicy(expiryPolicy == null ? TerminusExpiryPolicy.SESSION_END : expiryPolicy);
                case 5:
                    UnsignedInteger durable = (UnsignedInteger) l.get(1);
                    o.setDurable(durable == null ? TerminusDurability.NONE : durable);
                case 6:
                    o.setAddress( (String) l.get( 0 ) );
            }


            return o;
        }

        public Class<Target> getTypeClass()
        {
            return Target.class;
        }
    }


    public static void register(Decoder decoder)
    {
        TargetConstructor constructor = new TargetConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }

    @Override
    public String toString()
    {
        return "Target{" +
               "address='" + _address + '\'' +
               ", durable=" + _durable +
               ", expiryPolicy=" + _expiryPolicy +
               ", timeout=" + _timeout +
               ", dynamic=" + _dynamic +
               ", dynamicNodeProperties=" + _dynamicNodeProperties +
               ", capabilities=" + (_capabilities == null ? null : Arrays.asList(_capabilities)) +
               '}';
    }
}
  