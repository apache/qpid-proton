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
package org.apache.qpid.proton.amqp.messaging;

import java.util.Map;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedInteger;

public abstract class Terminus
{
    private String _address;
    private TerminusDurability _durable = TerminusDurability.NONE;
    private TerminusExpiryPolicy _expiryPolicy = TerminusExpiryPolicy.SESSION_END;
    private UnsignedInteger _timeout = UnsignedInteger.valueOf(0);
    private boolean _dynamic;
    private Map _dynamicNodeProperties;
    private Symbol[] _capabilities;

    Terminus()
    {
    }

    public final String getAddress()
    {
        return _address;
    }

    public final void setAddress(String address)
    {
        _address = address;
    }

    public final TerminusDurability getDurable()
    {
        return _durable;
    }

    public final void setDurable(TerminusDurability durable)
    {
        _durable = durable == null ? TerminusDurability.NONE : durable;
    }

    public final TerminusExpiryPolicy getExpiryPolicy()
    {
        return _expiryPolicy;
    }

    public final void setExpiryPolicy(TerminusExpiryPolicy expiryPolicy)
    {
        _expiryPolicy = expiryPolicy == null ? TerminusExpiryPolicy.SESSION_END : expiryPolicy;
    }

    public final UnsignedInteger getTimeout()
    {
        return _timeout;
    }

    public final void setTimeout(UnsignedInteger timeout)
    {
        _timeout = timeout;
    }

    public final boolean getDynamic()
    {
        return _dynamic;
    }

    public final void setDynamic(boolean dynamic)
    {
        _dynamic = dynamic;
    }

    public final Map getDynamicNodeProperties()
    {
        return _dynamicNodeProperties;
    }

    public final void setDynamicNodeProperties(Map dynamicNodeProperties)
    {
        _dynamicNodeProperties = dynamicNodeProperties;
    }


    public final Symbol[] getCapabilities()
    {
        return _capabilities;
    }

    public final void setCapabilities(Symbol... capabilities)
    {
        _capabilities = capabilities;
    }

}
