
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


package org.apache.qpid.proton.type;

import org.apache.qpid.proton.codec.Decoder;

public class AMQPDefinedTypes
{
    public static void registerAllTypes(Decoder decoder)
    {
        registerTransportTypes(decoder);
        registerMessagingTypes(decoder);
        registerTransactionTypes(decoder);
        registerSecurityTypes(decoder);
    }


    public static void registerTransportTypes(Decoder decoder)
    {
        org.apache.qpid.proton.type.transport.Open.register( decoder );
        org.apache.qpid.proton.type.transport.Begin.register( decoder );
        org.apache.qpid.proton.type.transport.Attach.register( decoder );
        org.apache.qpid.proton.type.transport.Flow.register( decoder );
        org.apache.qpid.proton.type.transport.Transfer.register( decoder );
        org.apache.qpid.proton.type.transport.Disposition.register( decoder );
        org.apache.qpid.proton.type.transport.Detach.register( decoder );
        org.apache.qpid.proton.type.transport.End.register( decoder );
        org.apache.qpid.proton.type.transport.Close.register( decoder );
        org.apache.qpid.proton.type.transport.Error.register( decoder );
    }

    public static void registerMessagingTypes(Decoder decoder)
    {
        org.apache.qpid.proton.type.messaging.Header.register( decoder );
        org.apache.qpid.proton.type.messaging.DeliveryAnnotations.register( decoder );
        org.apache.qpid.proton.type.messaging.MessageAnnotations.register( decoder );
        org.apache.qpid.proton.type.messaging.Properties.register( decoder );
        org.apache.qpid.proton.type.messaging.ApplicationProperties.register( decoder );
        org.apache.qpid.proton.type.messaging.Data.register( decoder );
        org.apache.qpid.proton.type.messaging.AmqpSequence.register( decoder );
        org.apache.qpid.proton.type.messaging.AmqpValue.register( decoder );
        org.apache.qpid.proton.type.messaging.Footer.register( decoder );
        org.apache.qpid.proton.type.messaging.Received.register( decoder );
        org.apache.qpid.proton.type.messaging.Accepted.register( decoder );
        org.apache.qpid.proton.type.messaging.Rejected.register( decoder );
        org.apache.qpid.proton.type.messaging.Released.register( decoder );
        org.apache.qpid.proton.type.messaging.Modified.register( decoder );
        org.apache.qpid.proton.type.messaging.Source.register( decoder );
        org.apache.qpid.proton.type.messaging.Target.register( decoder );
        org.apache.qpid.proton.type.messaging.DeleteOnClose.register( decoder );
        org.apache.qpid.proton.type.messaging.DeleteOnNoLinks.register( decoder );
        org.apache.qpid.proton.type.messaging.DeleteOnNoMessages.register( decoder );
        org.apache.qpid.proton.type.messaging.DeleteOnNoLinksOrMessages.register( decoder );
    }

    public static void registerTransactionTypes(Decoder decoder)
    {
        org.apache.qpid.proton.type.transaction.Coordinator.register( decoder );
        org.apache.qpid.proton.type.transaction.Declare.register( decoder );
        org.apache.qpid.proton.type.transaction.Discharge.register( decoder );
        org.apache.qpid.proton.type.transaction.Declared.register( decoder );
        org.apache.qpid.proton.type.transaction.TransactionalState.register( decoder );
    }

    public static void registerSecurityTypes(Decoder decoder)
    {
        org.apache.qpid.proton.type.security.SaslMechanisms.register( decoder );
        org.apache.qpid.proton.type.security.SaslInit.register( decoder );
        org.apache.qpid.proton.type.security.SaslChallenge.register( decoder );
        org.apache.qpid.proton.type.security.SaslResponse.register( decoder );
        org.apache.qpid.proton.type.security.SaslOutcome.register( decoder );
    }

}
