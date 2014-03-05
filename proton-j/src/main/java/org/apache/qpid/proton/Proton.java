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
import org.apache.qpid.proton.codec.Data;
import org.apache.qpid.proton.codec.DataFactory;
import org.apache.qpid.proton.driver.Driver;
import org.apache.qpid.proton.driver.DriverFactory;
import org.apache.qpid.proton.engine.Collector;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.EngineFactory;
import org.apache.qpid.proton.engine.SslDomain;
import org.apache.qpid.proton.engine.SslPeerDetails;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.message.Message;
import org.apache.qpid.proton.message.MessageFactory;
import org.apache.qpid.proton.messenger.Messenger;
import org.apache.qpid.proton.messenger.MessengerFactory;

import org.apache.qpid.proton.engine.impl.CollectorImpl;

public final class Proton
{

    public static ProtonFactory.ImplementationType ANY = ProtonFactory.ImplementationType.ANY;
    public static ProtonFactory.ImplementationType PROTON_C = ProtonFactory.ImplementationType.PROTON_C;
    public static ProtonFactory.ImplementationType PROTON_J = ProtonFactory.ImplementationType.PROTON_J;


    private static final MessengerFactory MESSENGER_FACTORY =
            (new ProtonFactoryLoader<MessengerFactory>(MessengerFactory.class)).loadFactory();
    private static final DriverFactory DRIVER_FACTORY =
            (new ProtonFactoryLoader<DriverFactory>(DriverFactory.class)).loadFactory();
    private static final MessageFactory MESSAGE_FACTORY =
            (new ProtonFactoryLoader<MessageFactory>(MessageFactory.class)).loadFactory();
    private static final DataFactory DATA_FACTORY =
            (new ProtonFactoryLoader<DataFactory>(DataFactory.class)).loadFactory();
    private static final EngineFactory ENGINE_FACTORY =
            (new ProtonFactoryLoader<EngineFactory>(EngineFactory.class)).loadFactory();

    private static final ProtonFactory.ImplementationType DEFAULT_IMPLEMENTATION =
            ProtonFactoryLoader.getImpliedImplementationType();

    private Proton()
    {
    }

    public static ProtonFactory.ImplementationType getDefaultImplementationType()
    {
        return DEFAULT_IMPLEMENTATION;
    }

    public static Collector collector()
    {
        return new CollectorImpl();
    }

    public static Connection connection()
    {
        return ENGINE_FACTORY.createConnection();
    }

    public static Transport transport()
    {
        return ENGINE_FACTORY.createTransport();
    }

    public static SslDomain sslDomain()
    {
        return ENGINE_FACTORY.createSslDomain();
    }

    public static SslPeerDetails sslPeerDetails(String hostname, int port)
    {
        return ENGINE_FACTORY.createSslPeerDetails(hostname, port);
    }

    public static Data data(long capacity)
    {
        return DATA_FACTORY.createData(capacity);
    }

    public static Message message()
    {
        return MESSAGE_FACTORY.createMessage();
    }

    public static Message message(Header header,
                      DeliveryAnnotations deliveryAnnotations, MessageAnnotations messageAnnotations,
                      Properties properties, ApplicationProperties applicationProperties,
                      Section body, Footer footer)
    {
        return MESSAGE_FACTORY.createMessage(header, deliveryAnnotations,
                                             messageAnnotations, properties,
                                             applicationProperties, body, footer);
    }


    public static Messenger messenger()
    {
        return MESSENGER_FACTORY.createMessenger();
    }

    public static Messenger messenger(String name)
    {
        return MESSENGER_FACTORY.createMessenger(name);
    }

    public static Driver driver() throws IOException
    {
        return DRIVER_FACTORY.createDriver();
    }



    public static Connection connection(ProtonFactory.ImplementationType implementation)
    {
        return getEngineFactory(implementation).createConnection();
    }

    public static Transport transport(ProtonFactory.ImplementationType implementation)
    {
        return getEngineFactory(implementation).createTransport();
    }

    public static SslDomain sslDomain(ProtonFactory.ImplementationType implementation)
    {
        return getEngineFactory(implementation).createSslDomain();
    }

    public static SslPeerDetails sslPeerDetails(ProtonFactory.ImplementationType implementation, String hostname, int port)
    {
        return getEngineFactory(implementation).createSslPeerDetails(hostname, port);
    }

    public static Data data(ProtonFactory.ImplementationType implementation, long capacity)
    {
        return getDataFactory(implementation).createData(capacity);
    }

    public static Message message(ProtonFactory.ImplementationType implementation)
    {
        return getMessageFactory(implementation).createMessage();
    }

    public static Message message(ProtonFactory.ImplementationType implementation, Header header,
                      DeliveryAnnotations deliveryAnnotations, MessageAnnotations messageAnnotations,
                      Properties properties, ApplicationProperties applicationProperties,
                      Section body, Footer footer)
    {
        return getMessageFactory(implementation).createMessage(header, deliveryAnnotations,
                                                               messageAnnotations, properties,
                                                               applicationProperties, body, footer);
    }


    public static Messenger messenger(ProtonFactory.ImplementationType implementation)
    {
        return getMessengerFactory(implementation).createMessenger();
    }

    public static Messenger messenger(ProtonFactory.ImplementationType implementation, String name)
    {
        return getMessengerFactory(implementation).createMessenger(name);
    }

    public static Driver driver(ProtonFactory.ImplementationType implementation) throws IOException
    {
        return getDriverFactory(implementation).createDriver();
    }


    private static final ConcurrentMap<ProtonFactory.ImplementationType, EngineFactory> _engineFactories =
            new ConcurrentHashMap<ProtonFactory.ImplementationType, EngineFactory>();
    private static final ConcurrentMap<ProtonFactory.ImplementationType, MessageFactory> _messageFactories =
            new ConcurrentHashMap<ProtonFactory.ImplementationType, MessageFactory>();
    private static final ConcurrentMap<ProtonFactory.ImplementationType, MessengerFactory> _messengerFactories =
                new ConcurrentHashMap<ProtonFactory.ImplementationType, MessengerFactory>();
    private static final ConcurrentMap<ProtonFactory.ImplementationType, DataFactory> _dataFactories =
                new ConcurrentHashMap<ProtonFactory.ImplementationType, DataFactory>();
    private static final ConcurrentMap<ProtonFactory.ImplementationType, DriverFactory> _driverFactories =
                new ConcurrentHashMap<ProtonFactory.ImplementationType, DriverFactory>();

    private static EngineFactory getEngineFactory(ProtonFactory.ImplementationType implementation)
    {
        return getFactory(EngineFactory.class, implementation, _engineFactories);
    }

    private static MessageFactory getMessageFactory(ProtonFactory.ImplementationType implementation)
    {
        return getFactory(MessageFactory.class, implementation, _messageFactories);
    }

    private static MessengerFactory getMessengerFactory(ProtonFactory.ImplementationType implementation)
    {
        return getFactory(MessengerFactory.class, implementation, _messengerFactories);
    }

    private static DriverFactory getDriverFactory(ProtonFactory.ImplementationType implementation)
    {
        return getFactory(DriverFactory.class, implementation, _driverFactories);
    }

    private static DataFactory getDataFactory(ProtonFactory.ImplementationType implementation)
    {
        return getFactory(DataFactory.class, implementation, _dataFactories);
    }

    private static <T extends ProtonFactory>  T getFactory(Class<T> factoryClass, ProtonFactory.ImplementationType implementation,
                                                           ConcurrentMap<ProtonFactory.ImplementationType, T> factories)
    {
        T factory = factories.get(implementation);
        if(factory == null)
        {
            factories.putIfAbsent(implementation, (new ProtonFactoryLoader<T>(factoryClass,implementation)).loadFactory());
            factory = factories.get(implementation);

        }
        return factory;
    }


}
