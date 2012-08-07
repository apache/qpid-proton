/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package org.apache.qpid.proton.driver.impl;

import java.util.UUID;

import org.apache.qpid.proton.driver.Connector;
import org.apache.qpid.proton.driver.Driver;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.Sasl;
import org.apache.qpid.proton.engine.Sasl.SaslState;
import org.apache.qpid.proton.engine.Sender;
import org.apache.qpid.proton.engine.Session;
import org.apache.qpid.proton.engine.impl.ConnectionImpl;
import org.apache.qpid.proton.logging.LogHandler;

public class Post
{
    enum State {NEW,AUTHENTICATING,CONNECTION_UP, FAILED};
    
    private LogHandler _logger;
    private Driver _driver;
    private Connector<State> _ctor;
    private Connection _conn;
    private Session _ssn;
    private Sender _sender;
    private String _mailbox;
    private String _msg;
    
    public Post(String mailbox, String msg) throws Exception
    {
        _mailbox = mailbox;
        _msg = msg;
        _logger = new SystemOutLogger("TestServer: ");
        _driver = new DriverImpl(_logger);
        
        // setup a driver connection to the server
        _ctor = _driver.createConnector("localhost", 5672, State.NEW);

        // configure SASL
        _ctor.sasl().setMechanisms(new String[]{"ANONYMOUS"});

        // inform the engine about the connection, and link the driver to it.
        _conn = new ConnectionImpl();
        ((ConnectionImpl)_conn).setLocalContainerId("Post");
        _ctor.setConnection(_conn);

        // create a session, and Link for receiving from the mailbox
        _logger.info("Posting to mailbox " + mailbox);
        _ssn = _conn.session();
        _sender = _ssn.sender("sender");
        _sender.setLocalTargetAddress(mailbox);

        // now open all the engine endpoints
        _conn.open();
        _ssn.open();
        _sender.open();
    }

    private void send() throws Exception
    {
        while (_sender.getCredit() == 0)
        {
            await();
        }
        
        _logger.info("Sending " + _msg + " to mailbox : " + _mailbox);
        
        String id = "post-delivery-" + UUID.randomUUID() ;
        _sender.delivery(id.getBytes(),0,id.getBytes().length);
        byte[] data = _msg.getBytes();
        _sender.send(data, 0, data.length);
        _sender.advance();
        
        await(); // wait for server to respond.
        
        //Delivery delivery = _sender.unsettled().next();
        Delivery delivery = _conn.getWorkHead();
        while (delivery == null)
        {
            await();
            delivery = _conn.getWorkHead();
        }
        
        if (delivery.getRemoteState() != null)
        {
            delivery.settle();
            _logger.info("Server has accepted msg");
        }
        else
        {
            _logger.info("Server has not accepted the msg!");
        }
    }
    
    private void close() throws Exception
    {
        _conn.close();
        while (_conn.getRemoteState() != EndpointState.CLOSED)
        {
            await();
        }
        _logger.info("Connection has been closed");
    }
    
    private void await() throws Exception
    {
        _logger.debug("Waiting for events...");

        // prepare pending outbound data for the network
        _ctor.process();

        // wait forever for network event(s)
        _driver.doWait(0);

        // process any data that arrived
        _ctor.process();
        
        _logger.debug("...waiting done!");
    }
    
	private boolean authenticate() throws Exception
	{
	    Sasl sasl = _ctor.sasl();
		while (sasl.getState() != SaslState.PN_SASL_PASS && sasl.getState() != SaslState.PN_SASL_FAIL)
		{
		    await();
		}
		
		return sasl.getState() == SaslState.PN_SASL_PASS;
	}
	
    public static void main(String[] args) throws Exception
    {
        if (args.length == 0)
        {
            System.out.println("You need to specify the mailbox and msg content.");
        }
        else if (args.length == 1)
        {
            System.out.println("You need to specify msg content.");
        }

        Post post = new Post(args[0],args[1]);
        
        if (post.authenticate())
        {
            System.out.println("Authentication sucessful");
        }
        else
        {
            System.out.println("Error: Authentication failure");
            return;
        }
        
        post.send();
        
        post.close();
    }

}
