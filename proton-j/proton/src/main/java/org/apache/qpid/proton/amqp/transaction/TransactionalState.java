
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


package org.apache.qpid.proton.amqp.transaction;

import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.transport.DeliveryState;
import org.apache.qpid.proton.amqp.messaging.Outcome;

public final class TransactionalState
      implements DeliveryState
{

    private Binary _txnId;
    private Outcome _outcome;

    public Binary getTxnId()
    {
        return _txnId;
    }

    public void setTxnId(Binary txnId)
    {
        if( txnId == null )
        {
            throw new NullPointerException("the txn-id field is mandatory");
        }

        _txnId = txnId;
    }

    public Outcome getOutcome()
    {
        return _outcome;
    }

    public void setOutcome(Outcome outcome)
    {
        _outcome = outcome;
    }

    @Override
    public String toString()
    {
        return "TransactionalState{" +
               "txnId=" + _txnId +
               ", outcome=" + _outcome +
               '}';
    }
}
  