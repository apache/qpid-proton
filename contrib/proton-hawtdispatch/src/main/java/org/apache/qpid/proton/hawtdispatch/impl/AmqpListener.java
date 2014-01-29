/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.apache.qpid.proton.hawtdispatch.impl;

import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.transport.ErrorCondition;
import org.apache.qpid.proton.engine.*;
import org.fusesource.hawtdispatch.Task;

import java.io.IOException;


/**
* @author <a href="http://hiramchirino.com">Hiram Chirino</a>
*/
public class AmqpListener {

    public Sasl processSaslConnect(ProtonJTransport protonTransport) {
        return null;
    }

    public Sasl processSaslEvent(Sasl sasl) {
        return sasl;
    }

    public void processRemoteOpen(Endpoint endpoint, Task onComplete) {
        ErrorCondition condition = endpoint.getCondition();
        condition.setCondition(Symbol.valueOf("error"));
        condition.setDescription("Not supported");
        endpoint.close();
        onComplete.run();
    }

    public void processRemoteClose(Endpoint endpoint, Task onComplete) {
        endpoint.close();
        onComplete.run();
    }

    public void processDelivery(Delivery delivery){
    }

    public void processTransportConnected() {
    }

    public void processTransportFailure(IOException e) {
        this.processFailure(e);
    }

    public void processFailure(Throwable e) {
        e.printStackTrace();
    }

    public void processRefill() {
    }

}
