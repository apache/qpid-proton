
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


package org.apache.qpid.proton.amqp.transport;

import java.util.Map;

import org.apache.qpid.proton.amqp.Symbol;


public final class ErrorCondition
{
    private Symbol _condition;
    private String _description;
    private Map _info;

    public ErrorCondition()
    {
    }

    public ErrorCondition(Symbol condition, String description)
    {
        _condition = condition;
        _description = description;
    }

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

    public void clear()
    {
        _condition = null;
        _description = null;
        _info = null;
    }

    public void copyFrom(ErrorCondition condition)
    {
        _condition = condition._condition;
        _description = condition._description;
        _info = condition._info;
    }

    @Override
    public boolean equals(Object o)
    {
        if (this == o)
        {
            return true;
        }
        if (o == null || getClass() != o.getClass())
        {
            return false;
        }

        ErrorCondition that = (ErrorCondition) o;

        if (_condition != null ? !_condition.equals(that._condition) : that._condition != null)
        {
            return false;
        }
        if (_description != null ? !_description.equals(that._description) : that._description != null)
        {
            return false;
        }
        if (_info != null ? !_info.equals(that._info) : that._info != null)
        {
            return false;
        }

        return true;
    }

    @Override
    public int hashCode()
    {
        int result = _condition != null ? _condition.hashCode() : 0;
        result = 31 * result + (_description != null ? _description.hashCode() : 0);
        result = 31 * result + (_info != null ? _info.hashCode() : 0);
        return result;
    }

    @Override
    public String toString()
    {
        return "Error{" +
               "condition=" + _condition +
               ", description='" + _description + '\'' +
               ", info=" + _info +
               '}';
    }
}
  