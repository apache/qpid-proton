
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


package org.apache.qpid.proton.amqp.security;



import org.apache.qpid.proton.amqp.Binary;


public final class SaslOutcome
      implements SaslFrameBody
{

    private SaslCode _code;
    private Binary _additionalData;

    public SaslCode getCode()
    {
        return _code;
    }

    public void setCode(SaslCode code)
    {
        if( code == null )
        {
            throw new NullPointerException("the code field is mandatory");
        }

        _code = code;
    }

    public Binary getAdditionalData()
    {
        return _additionalData;
    }

    public void setAdditionalData(Binary additionalData)
    {
        _additionalData = additionalData;
    }


    @Override
    public String toString()
    {
        return "SaslOutcome{" +
               "_code=" + _code +
               ", _additionalData=" + _additionalData +
               '}';
    }

    public <E> void invoke(SaslFrameBodyHandler<E> handler, Binary payload, E context)
    {
        handler.handleOutcome(this, payload, context);
    }



}
