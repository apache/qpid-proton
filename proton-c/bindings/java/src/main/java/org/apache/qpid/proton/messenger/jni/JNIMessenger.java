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
package org.apache.qpid.proton.messenger.jni;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.concurrent.TimeoutException;
import org.apache.qpid.proton.ProtonCEquivalent;
import org.apache.qpid.proton.ProtonUnsupportedOperationException;
import org.apache.qpid.proton.jni.Proton;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_message_t;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_messenger_t;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_subscription_t;
import org.apache.qpid.proton.jni.pn_status_t;
import org.apache.qpid.proton.message.Message;
import org.apache.qpid.proton.message.jni.JNIMessage;
import org.apache.qpid.proton.messenger.Messenger;
import org.apache.qpid.proton.messenger.MessengerException;
import org.apache.qpid.proton.messenger.Status;
import org.apache.qpid.proton.messenger.Tracker;

class JNIMessenger implements Messenger
{
    private SWIGTYPE_p_pn_messenger_t _impl;

    JNIMessenger()
    {
        this(java.util.UUID.randomUUID().toString());
    }

    JNIMessenger(final String name)
    {
        _impl = Proton.pn_messenger(name);
    }

    @Override
    public void put(final Message message) throws MessengerException
    {
        SWIGTYPE_p_pn_message_t message_t = (message instanceof JNIMessage) ? ((JNIMessage)message).getImpl() : convertMessage(message);
        int err = Proton.pn_messenger_put(_impl, message_t);
        if(err != 0)
        {
            //TODO - error handling
            throw new ProtonUnsupportedOperationException("Messenger error handling not yet implemented");
        }
    }

    private SWIGTYPE_p_pn_message_t convertMessage(final Message message)
    {
        int length = 512;
        int encoded;
        byte[] data;
        do
        {
            length = length*2;
            data = new byte[length];
            encoded = message.encode(data,0,length);
        }
        while (encoded == length || encoded < 0);

        final SWIGTYPE_p_pn_message_t message_t = Proton.pn_message();
        Proton.pn_message_decode(message_t, ByteBuffer.wrap(data, 0, encoded));

        return message_t;
    }

    @Override
    public void send() throws TimeoutException
    {
        int err = Proton.pn_messenger_send(_impl);
        if(err != 0)
        {
            //TODO - error handling
            throw new ProtonUnsupportedOperationException("Messenger error handling not yet implemented");
        }
    }

    @Override
    public void subscribe(final String source) throws MessengerException
    {
        SWIGTYPE_p_pn_subscription_t sub = Proton.pn_messenger_subscribe(_impl, source);
        // TODO
    }

    @Override
    public void recv(final int count) throws TimeoutException
    {
        int err = Proton.pn_messenger_recv(_impl, count);
        if(err != 0)
        {
            //TODO - error handling
            throw new ProtonUnsupportedOperationException("Messenger error handling not yet implemented");
        }
    }

    @Override
    public Message get()
    {
        SWIGTYPE_p_pn_message_t msg = Proton.pn_message();
        int err = Proton.pn_messenger_get(_impl, msg);
        if(err != 0)
        {
            //TODO - error handling... null?
            throw new ProtonUnsupportedOperationException("Messenger error handling not yet implemented");
        }
        return new JNIMessage(msg);
    }

    @Override
    public void start() throws IOException
    {
        int err = Proton.pn_messenger_start(_impl);
        if(err != 0)
        {
            //TODO - error handling
            throw new ProtonUnsupportedOperationException("Messenger error handling not yet implemented");
        }
    }

    @Override
    public void stop()
    {
        int err = Proton.pn_messenger_stop(_impl);
        if(err != 0)
        {
            //TODO - error handling
            throw new ProtonUnsupportedOperationException("Messenger error handling not yet implemented");
        }
    }

    @Override
    public void setTimeout(final long timeInMillis)
    {
        int err = Proton.pn_messenger_set_timeout(_impl, (int) timeInMillis);
        if(err != 0)
        {
            //TODO - error handling
            throw new ProtonUnsupportedOperationException("Messenger error handling not yet implemented");
        }
    }

    @Override
    public long getTimeout()
    {
        return Proton.pn_messenger_get_timeout(_impl);
    }

    @Override
    public int outgoing()
    {
        return Proton.pn_messenger_outgoing(_impl);
    }

    @Override
    public int incoming()
    {
        return Proton.pn_messenger_incoming(_impl);
    }

    @Override
    public int getIncomingWindow()
    {
        return Proton.pn_messenger_get_incoming_window(_impl);
    }

    @Override
    public void setIncomingWindow(final int window)
    {
        int err = Proton.pn_messenger_set_incoming_window(_impl, window);
        if(err != 0)
        {
            //TODO - error handling
            throw new ProtonUnsupportedOperationException("Messenger error handling not yet implemented");
        }
    }

    @Override
    public int getOutgoingWindow()
    {
        return Proton.pn_messenger_get_outgoing_window(_impl);
    }

    @Override
    public void setOutgoingWindow(final int window)
    {
        int err = Proton.pn_messenger_set_outgoing_window(_impl, window);
        if(err != 0)
        {
            //TODO - error handling
            throw new ProtonUnsupportedOperationException("Messenger error handling not yet implemented");
        }
    }

    @Override
    public Tracker incomingTracker()
    {
        return new JNITracker(Proton.pn_messenger_incoming_tracker(_impl));
    }

    @Override
    public Tracker outgoingTracker()
    {
        return new JNITracker(Proton.pn_messenger_outgoing_tracker(_impl));
    }

    @Override
    public void reject(final Tracker tracker, final int flags)
    {
        int err = Proton.pn_messenger_reject(_impl, ((JNITracker) tracker).getTracker(), flags);
        if(err != 0)
        {
            //TODO - error handling
            throw new ProtonUnsupportedOperationException("Messenger error handling not yet implemented");
        }
    }

    @Override
    public void accept(final Tracker tracker, final int flags)
    {
        int err = Proton.pn_messenger_accept(_impl, ((JNITracker) tracker).getTracker(), flags);
        if(err != 0)
        {
            //TODO - error handling
            throw new ProtonUnsupportedOperationException("Messenger error handling not yet implemented");
        }
    }

    @Override
    public void settle(final Tracker tracker, final int flags)
    {
        int err = Proton.pn_messenger_settle(_impl, ((JNITracker) tracker).getTracker(), flags);
        if(err != 0)
        {
            //TODO - error handling
            throw new ProtonUnsupportedOperationException("Messenger error handling not yet implemented");
        }
    }

    @Override
    public Status getStatus(final Tracker tracker)
    {
        pn_status_t status = Proton.pn_messenger_status(_impl, ((JNITracker) tracker).getTracker());
        if(status == pn_status_t.PN_STATUS_ACCEPTED)
        {
            return Status.ACCEPTED;
        }
        else if(status == pn_status_t.PN_STATUS_PENDING)
        {
            return Status.PENDING;
        }
        else if(status == pn_status_t.PN_STATUS_REJECTED)
        {
            return Status.REJECTED;
        }
        else if(status == pn_status_t.PN_STATUS_UNKNOWN)
        {
            return Status.UNKNOWN;
        }

        return Status.UNKNOWN;  //TODO - is this correct?
    }

    @Override
    protected void finalize() throws Throwable
    {
        free();
        super.finalize();
    }

    public void free()
    {
        if(_impl != null)
        {
            Proton.pn_messenger_free(_impl);
            _impl = null;
        }
    }

    private class JNITracker implements Tracker
    {
        private final long _tracker;

        public JNITracker(final long tracker)
        {
            _tracker = tracker;
        }

        public long getTracker()
        {
            return _tracker;
        }
    }
}
