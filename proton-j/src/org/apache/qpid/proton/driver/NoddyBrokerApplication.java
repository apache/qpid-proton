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

import java.util.EnumSet;
import org.apache.qpid.proton.engine.*;
import org.apache.qpid.proton.type.Binary;

public class NoddyBrokerApplication implements Application
{

    public static final EnumSet<EndpointState> UNINITIALIZED_SET = EnumSet.of(EndpointState.UNINITIALIZED);
    public static final EnumSet<EndpointState> INITIALIZED_SET = EnumSet.complementOf(UNINITIALIZED_SET);
    public static final EnumSet<EndpointState> ACTIVE_STATE = EnumSet.of(EndpointState.ACTIVE);
    public static final EnumSet<EndpointState> CLOSED_STATE = EnumSet.of(EndpointState.CLOSED);
    private byte _tagId;


    public void process(Connection conn)
    {
        if(conn.getLocalState() == EndpointState.UNINITIALIZED && conn.getRemoteState() != EndpointState.UNINITIALIZED)
        {
            conn.open();
        }
        Session session = conn.sessionHead(UNINITIALIZED_SET, INITIALIZED_SET);
        while(session != null)
        {
            // TODO
            session.open();
            session = conn.sessionHead(UNINITIALIZED_SET, INITIALIZED_SET);
        }

        Link link = conn.linkHead(UNINITIALIZED_SET, INITIALIZED_SET);
        while(link != null)
        {
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

        Delivery delivery = conn.getWorkHead();

        while(delivery != null)
        {
            System.out.println("received " + new Binary(delivery.getTag()));
            if(delivery.getLink() instanceof Receiver)
            {

                Receiver receiver = (Receiver) delivery.getLink();
                if(receiver.current() == null)
                {
                    receiver.advance();
                }
                if(receiver.recv(new byte[0], 0, 0) == -1)
                {

                    receiver.advance();

                }
                delivery.disposition(Accepted.getInstance());

            }
            else
            {
                Sender sender = (Sender) delivery.getLink();
                sendMessages(sender);
            }

            delivery = delivery.getWorkNext();
        }

        link = conn.linkHead(ACTIVE_STATE, CLOSED_STATE);
        while(link != null)
        {
            // TODO
            link.close();
            link = link.next(ACTIVE_STATE, CLOSED_STATE);
        }

        session = conn.sessionHead(ACTIVE_STATE, CLOSED_STATE);
        while(session != null)
        {
            //TODO - close links?
            session.close();
            session = session.next(ACTIVE_STATE, CLOSED_STATE);
        }
        if(conn.getLocalState() == EndpointState.ACTIVE && conn.getRemoteState() == EndpointState.CLOSED)
        {
                //TODO - close sessions / links?
                conn.close();
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
