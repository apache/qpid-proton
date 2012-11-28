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

import org.apache.qpid.proton.engine.Endpoint;
import org.apache.qpid.proton.engine.EndpointState;
import org.fusesource.hawtdispatch.Task;

/**
* @author <a href="http://hiramchirino.com">Hiram Chirino</a>
*/
public class EndpointContext {

    private final AmqpTransport transport;
    private final Endpoint endpoint;
    private Object attachment;
    boolean listenerProcessing;

    public EndpointContext(AmqpTransport transport, Endpoint endpoint) {
        this.transport = transport;
        this.endpoint = endpoint;
    }

    class ProcessedTask extends Task {
        @Override
        public void run() {
            transport.assertExecuting();
            listenerProcessing = false;
            transport.pumpOut();
        }
    }

    public void fireListenerEvents(AmqpListener listener) {
        if( listener!=null && !listenerProcessing ) {
            if( endpoint.getLocalState() == EndpointState.UNINITIALIZED &&
                endpoint.getRemoteState() != EndpointState.UNINITIALIZED ) {
                listenerProcessing = true;
                listener.processRemoteOpen(endpoint, new ProcessedTask());
            } else if( endpoint.getLocalState() == EndpointState.ACTIVE &&
                endpoint.getRemoteState() == EndpointState.CLOSED ) {
                listenerProcessing = true;
                listener.processRemoteClose(endpoint, new ProcessedTask());
            }
        }
        if( attachment !=null && attachment instanceof Task ) {
            ((Task) attachment).run();
        }
    }

    public Object getAttachment() {
        return attachment;
    }

    public <T> T getAttachment(Class<T> clazz) {
        return clazz.cast(getAttachment());
    }

    public void setAttachment(Object attachment) {
        this.attachment = attachment;
    }
}
