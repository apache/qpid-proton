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

package org.apache.qpid.proton.hawtdispatch.api;

import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.transport.ErrorCondition;
import org.apache.qpid.proton.hawtdispatch.impl.*;
import org.apache.qpid.proton.engine.Endpoint;
import org.apache.qpid.proton.engine.EndpointState;
import org.fusesource.hawtdispatch.Dispatch;
import org.fusesource.hawtdispatch.DispatchQueue;
import org.fusesource.hawtdispatch.Task;

/**
 * @author <a href="http://hiramchirino.com">Hiram Chirino</a>
 */
abstract class AmqpEndpointBase extends WatchBase {
    abstract protected Endpoint getEndpoint();
    abstract protected AmqpEndpointBase getParent();

    protected AmqpConnection getConnection() {
        return getParent().getConnection();
    }

    protected AmqpTransport getTransport() {
        return getConnection().transport;
    }

    protected DispatchQueue queue() {
        return getTransport().queue();
    }

    protected void assertExecuting() {
        getTransport().assertExecuting();
    }

    public void waitForRemoteOpen() throws Exception {
        assertNotOnDispatchQueue();
        getRemoteOpenFuture().await();
    }

    public Future<Void> getRemoteOpenFuture() {
        final Promise<Void> rc = new Promise<Void>();
        queue().execute(new Task() {
            @Override
            public void run() {
                onRemoteOpen(rc);
            }
        });
        return rc;
    }

    public void onRemoteOpen(final Callback<Void> cb) {
        addWatch(new Watch() {
            @Override
            public boolean execute() {
                switch (getEndpoint().getRemoteState()) {
                    case ACTIVE:
                        cb.onSuccess(null);
                        return true;
                    case CLOSED:
                        cb.onFailure(Support.illegalState("closed"));
                        return true;
                }
                return false;
            }
        });
    }

    public ErrorCondition waitForRemoteClose() throws Exception {
        assertNotOnDispatchQueue();
        return getRemoteCloseFuture().await();
    }

    public Future<ErrorCondition> getRemoteCloseFuture() {
        final Promise<ErrorCondition> rc = new Promise<ErrorCondition>();
        queue().execute(new Task() {
            @Override
            public void run() {
                onRemoteClose(rc);
            }
        });
        return rc;
    }

    public void onRemoteClose(final Callback<ErrorCondition> cb) {
        addWatch(new Watch() {
            @Override
            public boolean execute() {
                if (getEndpoint().getRemoteState() == EndpointState.CLOSED) {
                    cb.onSuccess(getEndpoint().getRemoteCondition());
                    return true;
                }
                return false;
            }
        });
    }

    public void close() {
        getEndpoint().close();
        pumpOut();
    }

    public EndpointState getRemoteState() {
        return getEndpoint().getRemoteState();
    }

    public ErrorCondition getRemoteError() {
        return getEndpoint().getRemoteCondition();
    }

    static protected ErrorCondition toError(Throwable value) {
        return new ErrorCondition(Symbol.valueOf("error"), value.toString());
    }

    class Attachment extends Task {
        AmqpEndpointBase endpoint() {
            return AmqpEndpointBase.this;
        }

        @Override
        public void run() {
            fireWatches();
        }
    }

    protected void attach() {
        getTransport().context(getEndpoint()).setAttachment(new Attachment());
    }

    protected void defer(Defer defer) {
        getTransport().defer(defer);
    }

    protected void pumpOut() {
        getTransport().pumpOut();
    }

    static protected void assertNotOnDispatchQueue() {
        assert Dispatch.getCurrentQueue()==null : "Not allowed to be called when executing on a dispatch queue";
    }

}
