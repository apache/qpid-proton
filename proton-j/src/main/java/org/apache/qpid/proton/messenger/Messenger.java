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
package org.apache.qpid.proton.messenger;

import java.io.IOException;

import org.apache.qpid.proton.TimeoutException;
import org.apache.qpid.proton.message.Message;

import org.apache.qpid.proton.messenger.impl.MessengerImpl;

/**
 *
 *  Messenger defines a high level interface for sending and receiving
 *  messages. Every Messenger contains a single logical queue of
 *  incoming messages and a single logical queue of outgoing
 *  messages. These messages in these queues may be destined for, or
 *  originate from, a variety of addresses.
 *
 *  <h3>Address Syntax</h3>
 *
 *  An address has the following form:
 *
 *    [ amqp[s]:// ] [user[:password]@] domain [/[name]]
 *
 *  Where domain can be one of:
 *
 *    host | host:port | ip | ip:port | name
 *
 *  The following are valid examples of addresses:
 *
 *   - example.org
 *   - example.org:1234
 *   - amqp://example.org
 *   - amqps://example.org
 *   - example.org/incoming
 *   - amqps://example.org/outgoing
 *   - amqps://fred:trustno1@example.org
 *   - 127.0.0.1:1234
 *   - amqps://127.0.0.1:1234
 *
 *  <h3>Sending &amp; Receiving Messages</h3>
 *
 *  The Messenger interface works in conjuction with the Message
 *  class. The Message class is a mutable holder of message content.
 *  The put method will encode the content in a given Message object
 *  into the outgoing message queue leaving that Message object free
 *  to be modified or discarded without having any impact on the
 *  content in the outgoing queue.
 *
 *  Similarly, the get method will decode the content in the incoming
 *  message queue into the supplied Message object.
*/
public interface Messenger
{

    public static final class Factory
    {
        public static Messenger create() {
            return new MessengerImpl();
        }

        public static Messenger create(String name) {
            return new MessengerImpl(name);
        }
    }

    /**
     * Flag for use with reject(), accept() and settle() methods.
     */
    static final int CUMULATIVE = 0x01;

    /**
     * Places the content contained in the message onto the outgoing
     * queue of the Messenger. This method will never block. The
     * send call may be used to block until the messages are
     * sent. Either a send() or a recv() call is neceesary at present
     * to cause the messages to actually be sent out.
     */
    void put(Message message) throws MessengerException;

    /**
     * Blocks until the outgoing queue is empty and, in the event that
     * an outgoing window has been set, until the messages in that
     * window have been received by the target to which they were
     * sent, or the operation times out. The timeout property
     * controls how long a Messenger will block before timing out.
     */
    void send() throws TimeoutException;

    void send(int n) throws TimeoutException;

    /**
     * Subscribes the Messenger to messages originating from the
     * specified source. The source is an address as specified in the
     * Messenger introduction with the following addition. If the
     * domain portion of the address begins with the '~' character,
     * the Messenger will interpret the domain as host/port, bind
     * to it, and listen for incoming messages. For example
     * "~0.0.0.0", "amqp://~0.0.0.0" will bind to any local interface
     * and listen for incoming messages.
     */
    void subscribe(String source) throws MessengerException;
    /**
     * Receives an arbitrary number of messages into the
     * incoming queue of the Messenger. This method will block until
     * at least one message is available or the operation times out.
     */
    void recv() throws TimeoutException;
    /**
     * Receives up to the specified number of messages into the
     * incoming queue of the Messenger. This method will block until
     * at least one message is available or the operation times out.
     */
    void recv(int count) throws TimeoutException;
    /**
     * Returns the capacity of the incoming message queue of
     * messenger. Note this count does not include those messages
     * already available on the incoming queue (see
     * incoming()). Rather it returns the number of incoming queue
     * entries available for receiving messages
     */
    int receiving();
    /**
     * Returns the message from the head of the incoming message
     * queue.
     */
    Message get();

    /**
     * Transitions the Messenger to an active state. A Messenger is
     * initially created in an inactive state. When inactive, a
     * Messenger will not send or receive messages from its internal
     * queues. A Messenger must be started before calling send() or
     * recv().
     */
    void start() throws IOException;
    /**
     * Transitions the Messenger to an inactive state. An inactive
     * Messenger will not send or receive messages from its internal
     * queues. A Messenger should be stopped before being discarded to
     * ensure a clean shutdown handshake occurs on any internally managed
     * connections.
     */
    void stop();

    boolean stopped();

    /** Sends or receives any outstanding messages queued for a
     * messenger.  If timeout is zero, no blocking is done.  A timeout
     * of -1 blocks forever, otherwise timeout is the maximum time (in
     * millisecs) to block.  Returns True if work was performed.
     */
    boolean work(long timeout) throws TimeoutException;

    void interrupt();

    void setTimeout(long timeInMillis);
    long getTimeout();

    boolean isBlocking();
    void setBlocking(boolean b);

    /**
     * Returns a count of the messages currently on the outgoing queue
     * (i.e. those that have been put() but not yet actually sent
     * out).
     */
    int outgoing();
    /**
     * Returns a count of the messages available on the incoming
     * queue.
     */
    int incoming();

    int getIncomingWindow();
    void setIncomingWindow(int window);

    int getOutgoingWindow();
    void setOutgoingWindow(int window);

    /**
     * Returns a token which can be used to accept or reject the
     * message returned in the previous get() call.
     */
    Tracker incomingTracker();
    /**
     * Returns a token which can be used to track the status of the
     * message of the previous put() call.
     */
    Tracker outgoingTracker();

    /**
     * Rejects messages retrieved from the incoming message queue. The
     * tracker object for a message is obtained through a call to
     * incomingTracker() following a get(). If the flags argument
     * contains CUMULATIVE, then all message up to the one identified
     * by the tracker will be rejected.
     */
    void reject(Tracker tracker, int flags);
    /**
     * Accepts messages retrieved from the incoming message queue. The
     * tracker object for a message is obtained through a call to
     * incomingTracker() following a get(). If the flags argument
     * contains CUMULATIVE, then all message up to the one identified
     * by the tracker will be accepted.
     */
    void accept(Tracker tracker, int flags);
    void settle(Tracker tracker, int flags);

    /**
     * Gets the last known remote state of the delivery associated
     * with the given tracker.
     */
    Status getStatus(Tracker tracker);

    void route(String pattern, String address);

    void rewrite(String pattern, String address);

    /**
     * Set the path to the certificate file.
     */
    void setCertificate(String certificate);

    /**
     * Get the path to the certificate file.
     */
    String getCertificate();

    /**
     * Set the path to private key file.
     */
    void setPrivateKey(String privateKey);

    /**
     * Get the path to the private key file.
     */
    String getPrivateKey();

    /**
     * Set the password for private key file.
     */
    void setPassword(String password);

    /**
     * Get the password for the priate key file.
     */
    String getPassword();

    /**
     * Set the path to the trusted certificate database.
     */
    void setTrustedCertificates(String trusted);

    /**
     * Get the path to the trusted certificate database.
     */
    String getTrustedCertificates();

}
