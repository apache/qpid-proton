
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

public final class Discharge
{

    private Binary _txnId;
    private Boolean _fail;

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

    public Boolean getFail()
    {
        return _fail;
    }

    public void setFail(Boolean fail)
    {
        _fail = fail;
    }

    @Override
    public String toString()
    {
        return "Discharge{" +
               "txnId=" + _txnId +
               ", fail=" + _fail +
               '}';
    }
}
  