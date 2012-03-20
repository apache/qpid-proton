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

package org.apache.qpid.proton.driver;

import org.apache.qpid.proton.engine.Accepted;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.engine.Endpoint;
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.Link;
import org.apache.qpid.proton.engine.Receiver;
import org.apache.qpid.proton.engine.Sender;
import org.apache.qpid.proton.engine.Sequence;
import org.apache.qpid.proton.engine.Session;
import org.apache.qpid.proton.type.Binary;

import java.util.EnumSet;

public class NoddyBrokerApplication implements Application
{

    public static final EnumSet<EndpointState> UNINITIALIZED_SET = EnumSet.of(EndpointState.UNINITIALIZED);
    public static final EnumSet<EndpointState> INITIALIZED_SET = EnumSet.complementOf(UNINITIALIZED_SET);
    public static final EnumSet<EndpointState> ACTIVE_STATE = EnumSet.of(EndpointState.ACTIVE);
    public static final EnumSet<EndpointState> CLOSED_STATE = EnumSet.of(EndpointState.CLOSED);
    private byte _tagId;


    public void process(Connection conn)
    {
        Sequence<? extends Endpoint> s = conn.endpoints(UNINITIALIZED_SET, INITIALIZED_SET);
        Endpoint endpoint;
        while((endpoint = s.next()) != null)
        {
            if(endpoint instanceof Connection)
            {
                // TODO
                conn.open();
            }
            else if(endpoint instanceof Session)
            {
                // TODO
                ((Session)endpoint).open();
            }
            else if(endpoint instanceof Link)
            {
                Link link = (Link) endpoint;

                // TODO

                // TODO - following is a hack
                link.setLocalSourceAddress(link.getRemoteSourceAddress());
                link.setLocalTargetAddress(link.getRemoteTargetAddress());

                link.open();
                if(link instanceof Receiver)
                {
                    ((Receiver)link).flow(200);
                }
                else
                {
                    Sender sender = (Sender)link;
                    sender.offer(100);
                    sendMessages(sender);
                }
            }
        }

        Sequence<? extends Delivery> deliveries = conn.getWorkSequence();
        Delivery delivery;
        while((delivery = deliveries.next()) != null)
        {
            System.out.println("received " + new Binary(delivery.getTag()));
            if(delivery.getLink() instanceof Receiver)
            {

                Receiver link = (Receiver) delivery.getLink();
                if(link.current() == null)
                {
                    link.advance();
                }
                if(link.recv(new byte[0], 0, 0) == -1)
                {

                    link.advance();
    
                }
                delivery.disposition(Accepted.getInstance());

            }
            else
            {
                Sender sender = (Sender) delivery.getLink();
                sendMessages(sender);
            }
        }

        s = conn.endpoints(ACTIVE_STATE, CLOSED_STATE);
        while((endpoint = s.next()) != null)
        {
            if(endpoint instanceof Link)
            {
                // TODO
                ((Link)endpoint).close();
            }
            else if(endpoint instanceof Session)
            {
                //TODO - close links?
                ((Session)endpoint).close();
            }
            else if(endpoint instanceof Connection)
            {
                //TODO - close sessions / links?
                ((Connection)endpoint).close();
            }

        }

    }

    private void sendMessages(Sender sender)
    {
        if(sender.current() == null)
        {
            byte[] tag = {_tagId++};
            sender.delivery(tag, 0, 1);

        }
        while(sender.advance())
        {
            

            System.out.println("Sending message " + _tagId);
            if(sender.current() == null)
            {
                byte[] tag = {_tagId++};
                sender.delivery(tag, 0, 1);
            }
        
        }
        ;
    }
}
