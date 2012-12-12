
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


package org.apache.qpid.proton.amqp.security;

import java.util.Arrays;
import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.Symbol;

public final class SaslMechanisms
      implements SaslFrameBody
{

    private Symbol[] _saslServerMechanisms;

    public Symbol[] getSaslServerMechanisms()
    {
        return _saslServerMechanisms;
    }

    public void setSaslServerMechanisms(Symbol... saslServerMechanisms)
    {
        if( saslServerMechanisms == null )
        {
            throw new NullPointerException("the sasl-server-mechanisms field is mandatory");
        }

        _saslServerMechanisms = saslServerMechanisms;
    }


    public <E> void invoke(SaslFrameBodyHandler<E> handler, Binary payload, E context)
    {
        handler.handleMechanisms(this, payload, context);
    }



    @Override
    public String toString()
    {
        return "SaslMechanisms{" +
               "saslServerMechanisms=" + (_saslServerMechanisms == null ? null : Arrays.asList(_saslServerMechanisms))
               +
               '}';
    }
}
