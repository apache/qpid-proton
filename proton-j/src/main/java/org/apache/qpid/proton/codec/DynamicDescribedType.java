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

import org.apache.qpid.proton.amqp.DescribedType;

import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

public class DynamicDescribedType implements AMQPType<DescribedType>
{

    private final EncoderImpl _encoder;
    private final Map<TypeEncoding, TypeEncoding> _encodings = new HashMap<TypeEncoding, TypeEncoding>();
    private final Object _descriptor;

    public DynamicDescribedType(EncoderImpl encoder, final Object descriptor)
    {
        _encoder = encoder;
        _descriptor = descriptor;
    }


    public Class<DescribedType> getTypeClass()
    {
        return DescribedType.class;
    }

    public TypeEncoding<DescribedType> getEncoding(final DescribedType val)
    {
        TypeEncoding underlyingEncoding = _encoder.getType(val.getDescribed()).getEncoding(val.getDescribed());
        TypeEncoding encoding = _encodings.get(underlyingEncoding);
        if(encoding == null)
        {
            encoding = new DynamicDescribedTypeEncoding(underlyingEncoding);
            _encodings.put(underlyingEncoding, encoding);
        }

        return encoding;
    }

    public TypeEncoding<DescribedType> getCanonicalEncoding()
    {
        return null;
    }

    public Collection<TypeEncoding<DescribedType>> getAllEncodings()
    {
        Collection values = _encodings.values();
        Collection unmodifiable = Collections.unmodifiableCollection(values);
        return (Collection<TypeEncoding<DescribedType>>) unmodifiable;
    }

    public void write(final DescribedType val)
    {
        TypeEncoding<DescribedType> encoding = getEncoding(val);
        encoding.writeConstructor();
        encoding.writeValue(val);
    }

    private class DynamicDescribedTypeEncoding implements TypeEncoding
    {
        private final TypeEncoding _underlyingEncoding;
        private final TypeEncoding _descriptorType;
        private final int _constructorSize;


        public DynamicDescribedTypeEncoding(final TypeEncoding underlyingEncoding)
        {
            _underlyingEncoding = underlyingEncoding;
            _descriptorType = _encoder.getType(_descriptor).getEncoding(_descriptor);
            _constructorSize = 1 + _descriptorType.getConstructorSize()
                               + _descriptorType.getValueSize(_descriptor)
                               + _underlyingEncoding.getConstructorSize();
        }

        public AMQPType getType()
        {
            return DynamicDescribedType.this;
        }

        public void writeConstructor()
        {
            _encoder.writeRaw(EncodingCodes.DESCRIBED_TYPE_INDICATOR);
            _descriptorType.writeConstructor();
            _descriptorType.writeValue(_descriptor);
            _underlyingEncoding.writeConstructor();
        }

        public int getConstructorSize()
        {
            return _constructorSize;
        }

        public void writeValue(final Object val)
        {
            _underlyingEncoding.writeValue(((DescribedType)val).getDescribed());
        }

        public int getValueSize(final Object val)
        {
            return _underlyingEncoding.getValueSize(((DescribedType) val).getDescribed());
        }

        public boolean isFixedSizeVal()
        {
            return _underlyingEncoding.isFixedSizeVal();
        }

        public boolean encodesSuperset(final TypeEncoding encoding)
        {
            return (getType() == encoding.getType())
                   && (_underlyingEncoding.encodesSuperset(((DynamicDescribedTypeEncoding)encoding)
                                                                   ._underlyingEncoding));
        }

        @Override
        public boolean encodesJavaPrimitive()
        {
            return false;
        }

    }
}
