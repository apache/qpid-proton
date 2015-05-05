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
package org.apache.qpid.proton.codec2;

import java.util.HashMap;
import java.util.Map;

import org.apache.qpid.proton.message2.Accepted;
import org.apache.qpid.proton.message2.AmqpSequence;
import org.apache.qpid.proton.message2.AmqpValue;
import org.apache.qpid.proton.message2.ApplicationProperties;
import org.apache.qpid.proton.message2.Data;
import org.apache.qpid.proton.message2.DeliveryAnnotations;
import org.apache.qpid.proton.message2.Footer;
import org.apache.qpid.proton.message2.Header;
import org.apache.qpid.proton.message2.LifetimePolicy;
import org.apache.qpid.proton.message2.MessageAnnotations;
import org.apache.qpid.proton.message2.Modified;
import org.apache.qpid.proton.message2.Received;
import org.apache.qpid.proton.message2.Rejected;
import org.apache.qpid.proton.message2.Released;
import org.apache.qpid.proton.security.SaslChallenge;
import org.apache.qpid.proton.security.SaslInit;
import org.apache.qpid.proton.security.SaslMechanisms;
import org.apache.qpid.proton.security.SaslOutcome;
import org.apache.qpid.proton.security.SaslResponse;
import org.apache.qpid.proton.transport.Attach;
import org.apache.qpid.proton.transport.Begin;
import org.apache.qpid.proton.transport.Close;
import org.apache.qpid.proton.transport.Detach;
import org.apache.qpid.proton.transport.Disposition;
import org.apache.qpid.proton.transport.End;
import org.apache.qpid.proton.transport.ErrorCondition;
import org.apache.qpid.proton.transport.Flow;
import org.apache.qpid.proton.transport.Open;
import org.apache.qpid.proton.transport.Source;
import org.apache.qpid.proton.transport.Target;
import org.apache.qpid.proton.transport.Transfer;

public class DiscriptorRegistry
{
    private static Map<String, DescribedTypeFactory> _typeRegByStrCode = new HashMap<String, DescribedTypeFactory>();

    private static Map<Long, DescribedTypeFactory> _typeRegByLongCode = new HashMap<Long, DescribedTypeFactory>();

    // Register the standard types
    static
    {
        registerTransportTypes();
        registerMessageTypes();
        registerSaslTypes();
    }

    public static void registerType(long longCode, String strCode, DescribedTypeFactory factory)
    {
        _typeRegByStrCode.put(strCode, factory);
        _typeRegByLongCode.put(longCode, factory);
    }

    private static void registerTransportTypes()
    {
        registerType(Open.DESCRIPTOR_LONG, Open.DESCRIPTOR_STRING, Open.FACTORY);
        registerType(Begin.DESCRIPTOR_LONG, Begin.DESCRIPTOR_STRING, Begin.FACTORY);
        registerType(Attach.DESCRIPTOR_LONG, Attach.DESCRIPTOR_STRING, Attach.FACTORY);
        registerType(Flow.DESCRIPTOR_LONG, Flow.DESCRIPTOR_STRING, Flow.FACTORY);
        registerType(Transfer.DESCRIPTOR_LONG, Transfer.DESCRIPTOR_STRING, Transfer.FACTORY);
        registerType(Disposition.DESCRIPTOR_LONG, Disposition.DESCRIPTOR_STRING, Disposition.FACTORY);
        registerType(Detach.DESCRIPTOR_LONG, Detach.DESCRIPTOR_STRING, Detach.FACTORY);
        registerType(End.DESCRIPTOR_LONG, End.DESCRIPTOR_STRING, End.FACTORY);
        registerType(Close.DESCRIPTOR_LONG, Close.DESCRIPTOR_STRING, Close.FACTORY);
        registerType(ErrorCondition.DESCRIPTOR_LONG, ErrorCondition.DESCRIPTOR_STRING, ErrorCondition.FACTORY);
    }

    private static void registerMessageTypes()
    {
        registerType(Header.DESCRIPTOR_LONG, Header.DESCRIPTOR_STRING, Header.FACTORY);
        registerType(DeliveryAnnotations.DESCRIPTOR_LONG, DeliveryAnnotations.DESCRIPTOR_STRING,
                DeliveryAnnotations.FACTORY);
        registerType(MessageAnnotations.DESCRIPTOR_LONG, MessageAnnotations.DESCRIPTOR_STRING,
                MessageAnnotations.FACTORY);
        registerType(ApplicationProperties.DESCRIPTOR_LONG, ApplicationProperties.DESCRIPTOR_STRING,
                ApplicationProperties.FACTORY);
        registerType(Data.DESCRIPTOR_LONG, Data.DESCRIPTOR_STRING, Data.FACTORY);
        registerType(AmqpSequence.DESCRIPTOR_LONG, AmqpSequence.DESCRIPTOR_STRING, AmqpSequence.FACTORY);
        registerType(AmqpValue.DESCRIPTOR_LONG, AmqpValue.DESCRIPTOR_STRING, AmqpValue.FACTORY);
        registerType(Footer.DESCRIPTOR_LONG, Footer.DESCRIPTOR_STRING, Footer.FACTORY);
        registerType(Accepted.DESCRIPTOR_LONG, Accepted.DESCRIPTOR_STRING, Accepted.FACTORY);
        registerType(Received.DESCRIPTOR_LONG, Received.DESCRIPTOR_STRING, Received.FACTORY);
        registerType(Rejected.DESCRIPTOR_LONG, Rejected.DESCRIPTOR_STRING, Rejected.FACTORY);
        registerType(Released.DESCRIPTOR_LONG, Released.DESCRIPTOR_STRING, Released.FACTORY);
        registerType(Modified.DESCRIPTOR_LONG, Modified.DESCRIPTOR_STRING, Modified.FACTORY);
        registerType(Source.DESCRIPTOR_LONG, Source.DESCRIPTOR_STRING, Source.FACTORY);
        registerType(Target.DESCRIPTOR_LONG, Target.DESCRIPTOR_STRING, Target.FACTORY);
        registerType(LifetimePolicy.DELETE_ON_CLOSE_TYPE.getLongDesc(),
                LifetimePolicy.DELETE_ON_CLOSE_TYPE.getStringDesc(), LifetimePolicy.DELETE_ON_CLOSE_TYPE);
        registerType(LifetimePolicy.DELETE_ON_NO_LINKS_TYPE.getLongDesc(),
                LifetimePolicy.DELETE_ON_NO_LINKS_TYPE.getStringDesc(), LifetimePolicy.DELETE_ON_NO_LINKS_TYPE);
        registerType(LifetimePolicy.DELETE_ON_NO_MSGS_TYPE.getLongDesc(),
                LifetimePolicy.DELETE_ON_NO_MSGS_TYPE.getStringDesc(), LifetimePolicy.DELETE_ON_NO_MSGS_TYPE);
        registerType(LifetimePolicy.DELETE_ON_NO_LINKS_OR_MSGS_TYPE.getLongDesc(),
                LifetimePolicy.DELETE_ON_NO_LINKS_OR_MSGS_TYPE.getStringDesc(),
                LifetimePolicy.DELETE_ON_NO_LINKS_OR_MSGS_TYPE);
    }

    public static void registerSaslTypes()
    {
        registerType(SaslMechanisms.DESCRIPTOR_LONG, SaslMechanisms.DESCRIPTOR_STRING, SaslMechanisms.FACTORY);
        registerType(SaslInit.DESCRIPTOR_LONG, SaslInit.DESCRIPTOR_STRING, SaslInit.FACTORY);
        registerType(SaslChallenge.DESCRIPTOR_LONG, SaslChallenge.DESCRIPTOR_STRING, SaslChallenge.FACTORY);
        registerType(SaslResponse.DESCRIPTOR_LONG, SaslResponse.DESCRIPTOR_STRING, SaslResponse.FACTORY);
        registerType(SaslOutcome.DESCRIPTOR_LONG, SaslOutcome.DESCRIPTOR_STRING, SaslOutcome.FACTORY);
    }

    public static DescribedTypeFactory lookup(Object code)
    {
        if (code instanceof Long)
        {
            return lookup((Long) code);
        }
        else if (code instanceof String)
        {
            return lookup((String) code);
        }
        else
        {
            return null;
        }
    }

    static DescribedTypeFactory lookup(long code)
    {
        return _typeRegByLongCode.get(code);
    }

    static DescribedTypeFactory lookup(String code)
    {
        return _typeRegByStrCode.get(code);
    }
}