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

abstract class AbstractPrimitiveTypeEncoding<T> implements PrimitiveTypeEncoding<T>
{
    private final EncoderImpl _encoder;
    private final DecoderImpl _decoder;

    AbstractPrimitiveTypeEncoding(final EncoderImpl encoder, final DecoderImpl decoder)
    {
        _encoder = encoder;
        _decoder = decoder;
    }

    public final void writeConstructor()
    {
        _encoder.writeRaw(getEncodingCode());
    }

    public int getConstructorSize()
    {
        return 1;
    }

    public abstract byte getEncodingCode();

    protected EncoderImpl getEncoder()
    {
        return _encoder;
    }

    public Class<T> getTypeClass()
    {
        return getType().getTypeClass();
    }

    protected DecoderImpl getDecoder()
    {
        return _decoder;
    }


    public boolean encodesJavaPrimitive()
    {
        return false;
    }

}
