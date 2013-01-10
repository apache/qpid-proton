/*
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
 */
package org.apache.qpid.proton.apireconciliation;

import java.lang.reflect.Method;

import org.apache.commons.lang.builder.EqualsBuilder;
import org.apache.commons.lang.builder.HashCodeBuilder;
import org.apache.commons.lang.builder.ToStringBuilder;

public class ReportRow
{
    private final String _declaredProtonCFunction;
    private final Method _javaMethod;

    public ReportRow(String declaredProtonCFunction, Method javaMethod)
    {
        _declaredProtonCFunction = declaredProtonCFunction;
        _javaMethod = javaMethod;
    }

    public String getCFunction()
    {
        return _declaredProtonCFunction;
    }

    public Method getJavaMethod()
    {
        return _javaMethod;
    }

    @Override
    public int hashCode()
    {
        return new HashCodeBuilder().append(_declaredProtonCFunction)
                                    .append(_javaMethod)
                                    .toHashCode();
    }

    @Override
    public boolean equals(Object obj)
    {
        if (obj == null)
        {
            return false;
        }
        if (obj == this)
        {
            return true;
        }
        if (obj.getClass() != getClass())
        {
            return false;
        }
        ReportRow rhs = (ReportRow) obj;
        return new EqualsBuilder()
                      .append(_declaredProtonCFunction, rhs._declaredProtonCFunction)
                      .append(_javaMethod, rhs._javaMethod)
                      .isEquals();
    }

    @Override
    public String toString()
    {
        return new ToStringBuilder(this)
                .append("_declaredProtonCFunction", _declaredProtonCFunction)
                .append("_javaMethod", _javaMethod)
                .toString();
    }

}
