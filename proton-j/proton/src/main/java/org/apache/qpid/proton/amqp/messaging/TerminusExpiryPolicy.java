
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

import java.util.HashMap;
import java.util.Map;
import org.apache.qpid.proton.amqp.Symbol;

public enum TerminusExpiryPolicy
{
    LINK_DETACH("link-detach"),
    SESSION_END("session-end"),
    CONNECTION_CLOSE("connection-close"),
    NEVER("never");

    private Symbol _policy;
    private static final Map<Symbol, TerminusExpiryPolicy> _map = new HashMap<Symbol, TerminusExpiryPolicy>();

    TerminusExpiryPolicy(String policy)
    {
        _policy = Symbol.valueOf(policy);
    }

    public Symbol getPolicy()
    {
        return _policy;
    }

    static
    {
        _map.put(LINK_DETACH.getPolicy(), LINK_DETACH);
        _map.put(SESSION_END.getPolicy(), SESSION_END);
        _map.put(CONNECTION_CLOSE.getPolicy(), CONNECTION_CLOSE);
        _map.put(NEVER.getPolicy(), NEVER);
    }

    public static TerminusExpiryPolicy valueOf(Symbol policy)
    {
        TerminusExpiryPolicy expiryPolicy = _map.get(policy);
        if(expiryPolicy == null)
        {
            throw new IllegalArgumentException("Unknown TerminusExpiryPolicy: " + policy);
        }
        return expiryPolicy;
    }
}
