
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

import org.apache.qpid.proton.amqp.transport.Attach;
import org.apache.qpid.proton.amqp.transport.Begin;
import org.apache.qpid.proton.amqp.transport.Close;
import org.apache.qpid.proton.amqp.transport.Detach;
import org.apache.qpid.proton.amqp.transport.Disposition;
import org.apache.qpid.proton.amqp.transport.End;
import org.apache.qpid.proton.amqp.transport.Flow;
import org.apache.qpid.proton.amqp.transport.Open;
import org.apache.qpid.proton.amqp.transport.Transfer;
import org.apache.qpid.proton.codec.messaging.*;
import org.apache.qpid.proton.codec.security.*;
import org.apache.qpid.proton.codec.transaction.*;
import org.apache.qpid.proton.codec.transport.*;

public class AMQPDefinedTypes
{
    public static void registerAllTypes(Decoder decoder, EncoderImpl encoder)
    {
        registerTransportTypes(decoder, encoder);
        registerMessagingTypes(decoder, encoder);
        registerTransactionTypes(decoder, encoder);
        registerSecurityTypes(decoder, encoder);
    }


    public static void registerTransportTypes(Decoder decoder, EncoderImpl encoder)
    {
        OpenType.register(decoder, encoder);
        BeginType.register(decoder, encoder);
        AttachType.register(decoder, encoder);
        FlowType.register(decoder, encoder);
        TransferType.register(decoder, encoder);
        DispositionType.register(decoder, encoder);
        DetachType.register(decoder, encoder);
        EndType.register(decoder, encoder);
        CloseType.register(decoder, encoder);
        ErrorConditionType.register(decoder, encoder);
    }

    public static void registerMessagingTypes(Decoder decoder, EncoderImpl encoder)
    {
        HeaderType.register(decoder, encoder);
        DeliveryAnnotationsType.register(decoder, encoder);
        MessageAnnotationsType.register(decoder, encoder);
        PropertiesType.register( decoder, encoder );
        ApplicationPropertiesType.register(decoder, encoder);
        DataType.register(decoder, encoder);
        AmqpSequenceType.register(decoder, encoder);
        AmqpValueType.register(decoder, encoder);
        FooterType.register(decoder, encoder);
        ReceivedType.register(decoder, encoder);
        AcceptedType.register(decoder , encoder);
        RejectedType.register(decoder, encoder);
        ReleasedType.register(decoder, encoder);
        ModifiedType.register(decoder, encoder);
        SourceType.register(decoder, encoder);
        TargetType.register(decoder, encoder);
        DeleteOnCloseType.register(decoder, encoder);
        DeleteOnNoLinksType.register(decoder, encoder);
        DeleteOnNoMessagesType.register(decoder, encoder);
        DeleteOnNoLinksOrMessagesType.register(decoder, encoder);
    }

    public static void registerTransactionTypes(Decoder decoder, EncoderImpl encoder)
    {
        CoordinatorType.register(decoder, encoder);
        DeclareType.register(decoder, encoder);
        DischargeType.register(decoder, encoder);
        DeclaredType.register(decoder, encoder);
        TransactionalStateType.register(decoder, encoder);
    }

    public static void registerSecurityTypes(Decoder decoder, EncoderImpl encoder)
    {
        SaslMechanismsType.register(decoder, encoder);
        SaslInitType.register(decoder, encoder);
        SaslChallengeType.register(decoder, encoder);
        SaslResponseType.register(decoder, encoder);
        SaslOutcomeType.register(decoder, encoder);
    }

}
