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
package org.apache.qpid.proton;

import java.io.IOException;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;
import org.apache.qpid.proton.amqp.messaging.ApplicationProperties;
import org.apache.qpid.proton.amqp.messaging.DeliveryAnnotations;
import org.apache.qpid.proton.amqp.messaging.Footer;
import org.apache.qpid.proton.amqp.messaging.Header;
import org.apache.qpid.proton.amqp.messaging.MessageAnnotations;
import org.apache.qpid.proton.amqp.messaging.Properties;
import org.apache.qpid.proton.amqp.messaging.Section;
import org.apache.qpid.proton.codec.Codec;
import org.apache.qpid.proton.codec.Data;
import org.apache.qpid.proton.driver.Driver;
import org.apache.qpid.proton.engine.Engine;
import org.apache.qpid.proton.engine.Collector;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.SslDomain;
import org.apache.qpid.proton.engine.SslPeerDetails;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.message.Message;
import org.apache.qpid.proton.messenger.Messenger;

public final class Proton
{

    private Proton()
    {
    }

    public static Collector collector()
    {
        return Engine.collector();
    }

    public static Connection connection()
    {
        return Engine.connection();
    }

    public static Transport transport()
    {
        return Engine.transport();
    }

    public static SslDomain sslDomain()
    {
        return Engine.sslDomain();
    }

    public static SslPeerDetails sslPeerDetails(String hostname, int port)
    {
        return Engine.sslPeerDetails(hostname, port);
    }

    public static Data data(long capacity)
    {
        return Codec.data(capacity);
    }

    public static Message message()
    {
        return Message.Factory.create();
    }

    public static Message message(Header header,
                      DeliveryAnnotations deliveryAnnotations, MessageAnnotations messageAnnotations,
                      Properties properties, ApplicationProperties applicationProperties,
                      Section body, Footer footer)
    {
        return Message.Factory.create(header, deliveryAnnotations,
                                      messageAnnotations, properties,
                                      applicationProperties, body, footer);
    }


    public static Messenger messenger()
    {
        return Messenger.Factory.create();
    }

    public static Messenger messenger(String name)
    {
        return Messenger.Factory.create(name);
    }

    public static Driver driver() throws IOException
    {
        return Driver.Factory.create();
    }

}
