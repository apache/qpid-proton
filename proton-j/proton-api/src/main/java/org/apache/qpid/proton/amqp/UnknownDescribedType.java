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
package org.apache.qpid.proton.amqp;

public class UnknownDescribedType implements DescribedType
{
    private final Object _descriptor;
    private final Object _described;

    public UnknownDescribedType(final Object descriptor, final Object described)
    {
        _descriptor = descriptor;
        _described = described;
    }

    @Override
    public Object getDescriptor()
    {
        return _descriptor;
    }

    @Override
    public Object getDescribed()
    {
        return _described;
    }

    @Override
    public boolean equals(final Object o)
    {
        if (this == o)
        {
            return true;
        }
        if (o == null || getClass() != o.getClass())
        {
            return false;
        }

        final UnknownDescribedType that = (UnknownDescribedType) o;

        if (_described != null ? !_described.equals(that._described) : that._described != null)
        {
            return false;
        }
        if (_descriptor != null ? !_descriptor.equals(that._descriptor) : that._descriptor != null)
        {
            return false;
        }

        return true;
    }

    @Override
    public int hashCode()
    {
        int result = _descriptor != null ? _descriptor.hashCode() : 0;
        result = 31 * result + (_described != null ? _described.hashCode() : 0);
        return result;
    }

    @Override
    public String toString()
    {
        return "UnknownDescribedType{" +
               "descriptor=" + _descriptor +
               ", described=" + _described +
               '}';
    }
}
