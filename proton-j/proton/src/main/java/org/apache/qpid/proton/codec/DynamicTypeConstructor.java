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
package org.apache.qpid.proton.codec;

public class DynamicTypeConstructor implements TypeConstructor
{
    private final DescribedTypeConstructor _describedTypeConstructor;
    private final TypeConstructor _underlyingEncoding;

    public DynamicTypeConstructor(final DescribedTypeConstructor dtc,
                                  final TypeConstructor underlyingEncoding)
    {
        _describedTypeConstructor = dtc;
        _underlyingEncoding = underlyingEncoding;
    }

    public Object readValue()
    {
        try
        {
            return _describedTypeConstructor.newInstance(_underlyingEncoding.readValue());
        }
        catch (NullPointerException npe)
        {
            throw new DecodeException("Unexpected null value - mandatory field not set? ("+npe.getMessage()+")", npe);
        }
        catch (ClassCastException cce)
        {
            throw new DecodeException("Incorrect type used", cce);
        }
    }

    public boolean encodesJavaPrimitive()
    {
        return false;
    }

    public Class getTypeClass()
    {
        return _describedTypeConstructor.getTypeClass();
    }
}
