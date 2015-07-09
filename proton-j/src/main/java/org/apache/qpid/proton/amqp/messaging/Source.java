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

import java.util.Arrays;
import java.util.Map;
import org.apache.qpid.proton.amqp.Symbol;

public final class Source extends Terminus
      implements org.apache.qpid.proton.amqp.transport.Source
{
    private Symbol _distributionMode;
    private Map _filter;
    private Outcome _defaultOutcome;
    private Symbol[] _outcomes;

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


    @Override
    public String toString()
    {
        return "Source{" +
               "address='" + getAddress() + '\'' +
               ", durable=" + getDurable() +
               ", expiryPolicy=" + getExpiryPolicy() +
               ", timeout=" + getTimeout() +
               ", dynamic=" + getDynamic() +
               ", dynamicNodeProperties=" + getDynamicNodeProperties() +
               ", distributionMode=" + _distributionMode +
               ", filter=" + _filter +
               ", defaultOutcome=" + _defaultOutcome +
               ", outcomes=" + (_outcomes == null ? null : Arrays.asList(_outcomes)) +
               ", capabilities=" + (getCapabilities() == null ? null : Arrays.asList(getCapabilities())) +
               '}';
    }
}
  