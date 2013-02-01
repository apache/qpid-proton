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
package org.apache.qpid.proton.message.jni;

import org.apache.qpid.proton.amqp.messaging.ApplicationProperties;
import org.apache.qpid.proton.amqp.messaging.DeliveryAnnotations;
import org.apache.qpid.proton.amqp.messaging.Footer;
import org.apache.qpid.proton.amqp.messaging.Header;
import org.apache.qpid.proton.amqp.messaging.MessageAnnotations;
import org.apache.qpid.proton.amqp.messaging.Properties;
import org.apache.qpid.proton.amqp.messaging.Section;
import org.apache.qpid.proton.jni.JNIFactory;
import org.apache.qpid.proton.message.Message;
import org.apache.qpid.proton.message.MessageFactory;

public class JNIMessageFactory extends JNIFactory implements MessageFactory
{

    @Override
    public Message createMessage()
    {
        return new JNIMessage();
    }

    @Override
    public Message createMessage(Header header,
                                 DeliveryAnnotations deliveryAnnotations, MessageAnnotations messageAnnotations,
                                 Properties properties, ApplicationProperties applicationProperties,
                                 Section body, Footer footer)
    {
        Message message = new JNIMessage();
        message.setHeader(header);
        message.setDeliveryAnnotations(deliveryAnnotations);
        message.setMessageAnnotations(messageAnnotations);
        message.setProperties(properties);
        message.setApplicationProperties(applicationProperties);
        message.setBody(body);
        message.setFooter(footer);

        return message;
    }

}
