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
package org.apache.qpid.proton.message.impl;

import org.apache.qpid.proton.ProtonFactoryImpl;
import org.apache.qpid.proton.amqp.messaging.ApplicationProperties;
import org.apache.qpid.proton.amqp.messaging.DeliveryAnnotations;
import org.apache.qpid.proton.amqp.messaging.Footer;
import org.apache.qpid.proton.amqp.messaging.Header;
import org.apache.qpid.proton.amqp.messaging.MessageAnnotations;
import org.apache.qpid.proton.amqp.messaging.Properties;
import org.apache.qpid.proton.amqp.messaging.Section;
import org.apache.qpid.proton.message.MessageFactory;
import org.apache.qpid.proton.message.ProtonJMessage;

public class MessageFactoryImpl extends ProtonFactoryImpl implements MessageFactory
{

    @SuppressWarnings("deprecation") // TODO remove once the constructor is made non-public (and therefore non-deprecated)
    @Override
    public ProtonJMessage createMessage()
    {
        return new MessageImpl();
    }

    @SuppressWarnings("deprecation") // TODO remove once the constructor is made non-public (and therefore non-deprecated)
    @Override
    public ProtonJMessage createMessage(Header header,
                                 DeliveryAnnotations deliveryAnnotations, MessageAnnotations messageAnnotations,
                                 Properties properties, ApplicationProperties applicationProperties,
                                 Section body, Footer footer)
    {
        return new MessageImpl(header,
                               deliveryAnnotations, messageAnnotations,
                               properties, applicationProperties,
                               body, footer);
    }
}
