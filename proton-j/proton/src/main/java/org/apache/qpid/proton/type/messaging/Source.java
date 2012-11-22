
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


public class Source
      implements DescribedType , org.apache.qpid.proton.type.transport.Source
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x0000000000000028L), Symbol.valueOf("amqp:source:list"), 
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x0000000000000028L);
    private final SourceWrapper _wrapper = new SourceWrapper();
    
    private String _address;
    private UnsignedInteger _durable = UnsignedInteger.valueOf(0);
    private Symbol _expiryPolicy = Symbol.valueOf("session-end");
    private UnsignedInteger _timeout = UnsignedInteger.valueOf(0);
    private boolean _dynamic;
    private Map _dynamicNodeProperties;
    private Symbol _distributionMode;
    private Map _filter;
    private Outcome _defaultOutcome;
    private Symbol[] _outcomes;
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

    public Symbol getDistributionMode()
    {
        return _distributionMode;
    }

    public void setDistributionMode(Symbol distributionMode)
    {
        _distributionMode = distributionMode;
    }

    public Map getFilter()
    {
        return _filter;
    }

    public void setFilter(Map filter)
    {
        _filter = filter;
    }

    public Outcome getDefaultOutcome()
    {
        return _defaultOutcome;
    }

    public void setDefaultOutcome(Outcome defaultOutcome)
    {
        _defaultOutcome = defaultOutcome;
    }

    public Symbol[] getOutcomes()
    {
        return _outcomes;
    }

    public void setOutcomes(Symbol... outcomes)
    {
        _outcomes = outcomes;
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
                return _distributionMode;
            case 7:
                return _filter;
            case 8:
                return _defaultOutcome;
            case 9:
                return _outcomes;
            case 10:
                return _capabilities;            
        }

        throw new IllegalStateException("Unknown index " + index);

    }

    public int size()
    {
        return _capabilities != null 
                  ? 11 
                  : _outcomes != null 
                  ? 10 
                  : _defaultOutcome != null 
                  ? 9 
                  : _filter != null 
                  ? 8 
                  : _distributionMode != null 
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


    public final class SourceWrapper extends AbstractList
    {

        @Override
        public Object get(final int index)
        {
            return Source.this.get(index);
        }

        @Override
        public int size()
        {
            return Source.this.size();
        }
    }

    private static class SourceConstructor implements DescribedTypeConstructor<Source>
    {
        public Source newInstance(Object described)
        {
            List l = (List) described;

            Source o = new Source();


            switch(11 - l.size())
            {

                case 0:
                    Object val0 = l.get( 10 );
                    if( val0 == null || val0.getClass().isArray() )
                    {
                        o.setCapabilities( (Symbol[]) val0 );
                    }
                    else
                    {
                        o.setCapabilities( (Symbol) val0 );
                    }
                case 1:
                    Object val1 = l.get( 9 );
                    if( val1 == null || val1.getClass().isArray() )
                    {
                        o.setOutcomes( (Symbol[]) val1 );
                    }
                    else
                    {
                        o.setOutcomes( (Symbol) val1 );
                    }
                case 2:
                    o.setDefaultOutcome( (Outcome) l.get( 8 ) );
                case 3:
                    o.setFilter( (Map) l.get( 7 ) );
                case 4:
                    o.setDistributionMode( (Symbol) l.get( 6 ) );
                case 5:
                    o.setDynamicNodeProperties( (Map) l.get( 5 ) );
                case 6:
                    Boolean dynamic = (Boolean) l.get(4);
                    o.setDynamic(dynamic == null ? false : dynamic);
                case 7:
                    UnsignedInteger timeout = (UnsignedInteger) l.get(3);
                    o.setTimeout(timeout == null ? UnsignedInteger.ZERO : timeout);
                case 8:
                    Symbol expiryPolicy = (Symbol) l.get(2);
                    o.setExpiryPolicy(expiryPolicy == null ? TerminusExpiryPolicy.SESSION_END : expiryPolicy);
                case 9:
                    UnsignedInteger durable = (UnsignedInteger) l.get(1);
                    o.setDurable(durable == null ? TerminusDurability.NONE : durable);
                case 10:
                    o.setAddress( (String) l.get( 0 ) );
            }


            return o;
        }

        public Class<Source> getTypeClass()
        {
            return Source.class;
        }
    }


    public static void register(Decoder decoder)
    {
        SourceConstructor constructor = new SourceConstructor();
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, constructor);
        }
    }

    @Override
    public String toString()
    {
        return "Source{" +
               "address='" + _address + '\'' +
               ", durable=" + _durable +
               ", expiryPolicy=" + _expiryPolicy +
               ", timeout=" + _timeout +
               ", dynamic=" + _dynamic +
               ", dynamicNodeProperties=" + _dynamicNodeProperties +
               ", distributionMode=" + _distributionMode +
               ", filter=" + _filter +
               ", defaultOutcome=" + _defaultOutcome +
               ", outcomes=" + (_outcomes == null ? null : Arrays.asList(_outcomes)) +
               ", capabilities=" + (_capabilities == null ? null : Arrays.asList(_capabilities)) +
               '}';
    }
}
  