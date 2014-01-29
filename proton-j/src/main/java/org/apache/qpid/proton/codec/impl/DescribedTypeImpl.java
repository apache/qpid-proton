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

package org.apache.qpid.proton.codec.impl;

import org.apache.qpid.proton.amqp.DescribedType;

class DescribedTypeImpl implements DescribedType
{
    private final Object _descriptor;
    private final Object _described;

    public DescribedTypeImpl(final Object descriptor, final Object described)
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
    public boolean equals(Object o)
    {
        if (this == o)
        {
            return true;
        }
        if (o == null || ! (o instanceof DescribedType))
        {
            return false;
        }

        DescribedType that = (DescribedType) o;

        if (_described != null ? !_described.equals(that.getDescribed()) : that.getDescribed() != null)
        {
            return false;
        }
        if (_descriptor != null ? !_descriptor.equals(that.getDescriptor()) : that.getDescriptor() != null)
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
        return "{"  + _descriptor +
               ": " + _described +
               '}';
    }
}