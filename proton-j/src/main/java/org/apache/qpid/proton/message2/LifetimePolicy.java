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
package org.apache.qpid.proton.message2;

import org.apache.qpid.proton.codec2.DecodeException;
import org.apache.qpid.proton.codec2.DescribedTypeFactory;
import org.apache.qpid.proton.codec2.Encodable;
import org.apache.qpid.proton.codec2.Encoder;

public enum LifetimePolicy implements Encodable, DescribedTypeFactory
{
    DELETE_ON_CLOSE_TYPE(0x000000000000002bL, "amqp:delete-on-close:list"), 
    DELETE_ON_NO_LINKS_TYPE(0x000000000000002cL, "amqp:delete-on-no-links:list"),
    DELETE_ON_NO_MSGS_TYPE(0x000000000000002dL, "amqp:delete-on-no-messages:list"),
    DELETE_ON_NO_LINKS_OR_MSGS_TYPE(0x000000000000002eL, "amqp:delete-on-no-links-or-messages:list")
    ;

    public final long _descLong;

    public final String _descString;

    LifetimePolicy(long descLong, String descString)
    {
        _descLong = descLong;
        _descString = descString;
    }

    @Override
    public void encode(Encoder encoder)
    {
        encoder.putDescriptor();
        encoder.putUlong(_descLong);
        encoder.putList();
        encoder.end();
    }

    @Override
    public Object create(Object in) throws DecodeException
    {
        return this;
    }
}