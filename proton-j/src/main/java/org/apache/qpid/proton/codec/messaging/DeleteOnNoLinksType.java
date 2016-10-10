
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


package org.apache.qpid.proton.codec.messaging;

import java.util.Collections;
import java.util.List;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.messaging.DeleteOnNoLinks;
import org.apache.qpid.proton.codec.AbstractDescribedType;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;
import org.apache.qpid.proton.codec.ReadableBuffer;
import org.apache.qpid.proton.codec.TypeConstructor;

public class DeleteOnNoLinksType extends AbstractDescribedType<DeleteOnNoLinks,List> implements DescribedTypeConstructor<DeleteOnNoLinks>
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x000000000000002cL), Symbol.valueOf("amqp:delete-on-no-links:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x000000000000002cL);

    private DeleteOnNoLinksType(EncoderImpl encoder)
    {
        super(encoder);
    }

    public UnsignedLong getDescriptor()
    {
        return DESCRIPTOR;
    }

    @Override
    protected List wrap(DeleteOnNoLinks val)
    {
        return Collections.EMPTY_LIST;
    }

    @Override
    public DeleteOnNoLinks newInstance(ReadableBuffer buffer,  TypeConstructor constructor)
    {
        // it's not used but we have to read it to make sure it won't damage the sequence
        constructor.readValue(buffer);
        return DeleteOnNoLinks.getInstance();
    }

    @Override
    public Class<DeleteOnNoLinks> getTypeClass()
    {
        return DeleteOnNoLinks.class;
    }



    public static void register(Decoder decoder, EncoderImpl encoder)
    {
        DeleteOnNoLinksType type = new DeleteOnNoLinksType(encoder);
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }
}
  