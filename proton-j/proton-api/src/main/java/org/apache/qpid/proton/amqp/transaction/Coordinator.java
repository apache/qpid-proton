
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


package org.apache.qpid.proton.amqp.transaction;

import java.util.Arrays;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.transport.Target;


public final class Coordinator
      implements Target
{
    private Symbol[] _capabilities;

    public Symbol[] getCapabilities()
    {
        return _capabilities;
    }

    public void setCapabilities(Symbol... capabilities)
    {
        _capabilities = capabilities;
    }

    @Override
    public String toString()
    {
        return "Coordinator{" +
               "capabilities=" + (_capabilities == null ? null : Arrays.asList(_capabilities)) +
               '}';
    }

    public String getAddress()
    {
        return null;
    }
}

