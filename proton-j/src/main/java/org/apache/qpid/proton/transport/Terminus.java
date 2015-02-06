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

import java.util.Map;

public abstract class Terminus
{
    protected String _address;

    protected TerminusDurability _durable = TerminusDurability.NONE;

    protected TerminusExpiryPolicy _expiryPolicy = TerminusExpiryPolicy.SESSION_END;

    protected long _timeout = 0;

    protected boolean _dynamic;

    protected Map<Object, Object> _dynamicNodeProperties;

    protected String[] _capabilities;

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

    public final long getTimeout()
    {
        return _timeout;
    }

    public final void setTimeout(long timeout)
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

    public final Map<Object, Object> getDynamicNodeProperties()
    {
        return _dynamicNodeProperties;
    }

    public final void setDynamicNodeProperties(Map<Object, Object> dynamicNodeProperties)
    {
        _dynamicNodeProperties = dynamicNodeProperties;
    }

    public final String[] getCapabilities()
    {
        return _capabilities;
    }

    public final void setCapabilities(String... capabilities)
    {
        _capabilities = capabilities;
    }
}