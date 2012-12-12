
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
import org.apache.qpid.proton.amqp.messaging.DeleteOnNoMessages;
import org.apache.qpid.proton.codec.AbstractDescribedType;
import org.apache.qpid.proton.codec.Decoder;
import org.apache.qpid.proton.codec.DescribedTypeConstructor;
import org.apache.qpid.proton.codec.EncoderImpl;


public class DeleteOnNoMessagesType extends AbstractDescribedType<DeleteOnNoMessages,List> implements DescribedTypeConstructor<DeleteOnNoMessages>
{
    private static final Object[] DESCRIPTORS =
    {
        UnsignedLong.valueOf(0x000000000000002dL), Symbol.valueOf("amqp:delete-on-no-messages:list"),
    };

    private static final UnsignedLong DESCRIPTOR = UnsignedLong.valueOf(0x000000000000002dL);

    private DeleteOnNoMessagesType(EncoderImpl encoder)
    {
        super(encoder);
    }

    public UnsignedLong getDescriptor()
    {
        return DESCRIPTOR;
    }

    @Override
    protected List wrap(DeleteOnNoMessages val)
    {
        return Collections.EMPTY_LIST;
    }

    @Override
    public DeleteOnNoMessages newInstance(Object described)
    {
        return DeleteOnNoMessages.getInstance();
    }

    @Override
    public Class<DeleteOnNoMessages> getTypeClass()
    {
        return DeleteOnNoMessages.class;
    }

    public static void register(Decoder decoder, EncoderImpl encoder)
    {
        DeleteOnNoMessagesType type = new DeleteOnNoMessagesType(encoder);
        for(Object descriptor : DESCRIPTORS)
        {
            decoder.register(descriptor, type);
        }
        encoder.register(type);
    }
}
  