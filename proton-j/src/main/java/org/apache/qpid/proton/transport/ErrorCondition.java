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

import java.util.List;
import java.util.Map;

import org.apache.qpid.proton.codec2.CodecHelper;
import org.apache.qpid.proton.codec2.DecodeException;
import org.apache.qpid.proton.codec2.DescribedTypeFactory;
import org.apache.qpid.proton.codec2.Encodable;
import org.apache.qpid.proton.codec2.Encoder;

public final class ErrorCondition implements Encodable
{
    public final static long DESCRIPTOR_LONG = 0x000000000000001dL;

    public final static String DESCRIPTOR_STRING = "amqp:error:list";

    private String _condition;

    private String _description;

    private Map<Object, Object> _info;

    public ErrorCondition()
    {
    }

    public ErrorCondition(String condition, String description)
    {
        _condition = condition;
        _description = description;
    }

    public String getCondition()
    {
        return _condition;
    }

    public void setCondition(String condition)
    {
        if (condition == null)
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

    public Map<Object, Object> getInfo()
    {
        return _info;
    }

    public void setInfo(Map<Object, Object> info)
    {
        _info = info;
    }

    public void clear()
    {
        _condition = null;
        _description = null;
        _info = null;
    }

    @Override
    public void encode(Encoder encoder)
    {
        encoder.putDescriptor();
        encoder.putUlong(DESCRIPTOR_LONG);
        encoder.putList();
        encoder.putSymbol(_condition);
        encoder.putString(_description);
        CodecHelper.encodeMap(encoder, _info);
        encoder.end();
    }

    public static final class Factory implements DescribedTypeFactory
    {
        @SuppressWarnings("unchecked")
        public Object create(Object in) throws DecodeException
        {
            List<Object> l = (List<Object>) in;
            ErrorCondition error = new ErrorCondition();

            switch (3 - l.size())
            {
            case 0:
                error.setInfo((Map<Object, Object>) l.get(2));
            case 1:
                error.setDescription((String) l.get(1));
            case 2:
                error.setCondition((String) l.get(0));
            }
            return error;
        }
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
        return "Error{" + "condition=" + _condition + ", description='" + _description + '\'' + ", info=" + _info + '}';
    }
}