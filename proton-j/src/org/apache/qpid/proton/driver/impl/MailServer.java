package org.apache.qpid.proton.driver.impl;

import java.io.ByteArrayOutputStream;
import java.util.ArrayList;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.apache.qpid.proton.driver.Connector;
import org.apache.qpid.proton.driver.Driver;
import org.apache.qpid.proton.driver.Listener;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.Link;
import org.apache.qpid.proton.engine.Receiver;
import org.apache.qpid.proton.engine.Sasl;
import org.apache.qpid.proton.engine.Sasl.SaslOutcome;
import org.apache.qpid.proton.engine.Sasl.SaslState;
import org.apache.qpid.proton.engine.Sender;
import org.apache.qpid.proton.engine.Session;
import org.apache.qpid.proton.engine.impl.ConnectionImpl;
import org.apache.qpid.proton.engine.impl.SaslServerImpl;
import org.apache.qpid.proton.logging.LogHandler;
import org.apache.qpid.proton.type.transport.DeliveryState;

public class MailServer
{
    enum State {NEW,AUTHENTICATING,CONNECTION_UP, FAILED};

    private Driver _driver;
    private LogHandler _logger;
    private Listener<State> _listener;
    private String[] _saslMechs = "ANONYMOUS".split(",");
    private int _counter;
    private Map<String,List<byte[]>> _mailboxes = new HashMap<String,List<byte[]>>();

    public MailServer() throws Exception
    {
        _logger = new Logger();
        _driver = new DriverImpl(_logger);
        _listener = _driver.createListener("localhost", 5672, State.NEW);
    }

    public void doWait()
    {
        _driver.doWait(0);
    }

    public void acceptConnections()
    {
        // We have only one listener
        if (_driver.listener() != null)
        {
            _logger.info("Accepting Connection.");
            Connector<State> ctor = _listener.accept();
            ctor.sasl().setMechanisms(_saslMechs);
            ctor.setContext(State.AUTHENTICATING);
        }
    }

    public void processConnections() throws Exception
    {
        Connector<State> ctor = _driver.connector();
        while (ctor != null)
        {
            // process any data coming from the network, this will update the
            // engine's view of the state of the remote clients
            ctor.process();
            State state = ctor.getContext();
            if (state == State.AUTHENTICATING)
            {
                //connection has not passed SASL authentication yet
                authenticateConnector(ctor);
            }
            else if (state == State.CONNECTION_UP)
            {
                //active connection, service any engine events
                serviceConnector(ctor);
            }
            else
            {
                _logger.error("Unknown state.");
            }

            // now generate any outbound network data generated in response to
            // any work done by the engine.
            ctor.process();
            if (ctor.isClosed())
            {
                ctor.destroy();
            }

            ctor = _driver.connector();
        }
    }

    private void authenticateConnector(Connector<State> ctor) throws Exception
    {
        _logger.info("Authenticating...");
        Sasl sasl = ctor.sasl();
        SaslState state = sasl.getState();
        while (state == SaslState.PN_SASL_CONF || state == SaslState.PN_SASL_STEP)
        {
            if (state == SaslState.PN_SASL_CONF)
            {
                _logger.info("Authenticating-CONF...");
                sasl.setMechanisms(new String[]{"ANONYMOUS"});
            }
            else if (state == SaslState.PN_SASL_STEP)
            {
                _logger.info("Authenticating-STEP...");
                String[] mechs = sasl.getRemoteMechanisms();
                if (mechs[0] == "ANONYMOUS")
                {
                    ((SaslServerImpl)sasl).done(SaslOutcome.PN_SASL_OK);
                }
                else
                {
                    //sasl.(sasl, PN_SASL_AUTH)
                }
            }
            state = sasl.getState();
        }

        if (state == SaslState.PN_SASL_PASS)
        {
            ctor.setConnection(new ConnectionImpl());
            ctor.setContext(State.CONNECTION_UP);
            _logger.info("Authentication-PASSED");
        }
        else if (state == SaslState.PN_SASL_FAIL)
        {
            ctor.setContext(State.FAILED);
            ctor.close();
            _logger.info("Authentication-FAILED");
        }
        else
        {
            _logger.info("Authentication-PENDING");
        }

    }

    private void serviceConnector(Connector<State> ctor) throws Exception
    {
       Connection con = ctor.getConnection();

       // Step 1: setup the engine's connection, and any sessions and links
       // that may be pending.

       // initialize the connection if it's new
       if (con.getLocalState() == EndpointState.UNINITIALIZED)
       {
           con.open();
           _logger.debug("Connection Opened.");
       }

       // open all pending sessions
       Session ssn = con.sessionHead(EnumSet.of(EndpointState.UNINITIALIZED),
               EnumSet.of(EndpointState.UNINITIALIZED,EndpointState.ACTIVE));
       while (ssn != null)
       {

           ssn.open();
           _logger.debug("Session Opened.");
           ssn = con.sessionHead(EnumSet.of(EndpointState.UNINITIALIZED),
           EnumSet.of(EndpointState.UNINITIALIZED,EndpointState.ACTIVE));
       }

       // configure and open any pending links
       Link link = con.linkHead(EnumSet.of(EndpointState.UNINITIALIZED),
               EnumSet.of(EndpointState.UNINITIALIZED,EndpointState.ACTIVE));
       while (link != null)
       {

           setupLink(link);
           _logger.debug("Link Opened.");
           link = con.linkHead(EnumSet.of(EndpointState.UNINITIALIZED),
           EnumSet.of(EndpointState.UNINITIALIZED,EndpointState.ACTIVE));
       }

       // Step 2: Now drain all the pending deliveries from the connection's
       // work queue and process them

       Delivery delivery = con.getWorkHead();
       while (delivery != null)
       {
           _logger.debug("Process delivery " + String.valueOf(delivery.getTag()));

           if (delivery.isReadable())   // inbound data available
           {
               processReceive(delivery);
           }
           else if (delivery.isWritable()) // can send a message
           {
               sendMessage(delivery);
           }

           // check to see if the remote has accepted message we sent
           if (delivery.getRemoteState() != null)
           {
               _logger.debug("Remote has seen it, Settling delivery " + String.valueOf(delivery.getTag()));
               // once we know the remote has seen the message, we can
               // release the delivery.
               delivery.settle();
           }

           delivery = delivery.getWorkNext();
       }

       // Step 3: Clean up any links or sessions that have been closed by the
       // remote.  If the connection has been closed remotely, clean that up also.

       // teardown any terminating links
       link = con.linkHead(EnumSet.of(EndpointState.ACTIVE),
               EnumSet.of(EndpointState.CLOSED));
       while (link != null)
       {
           link.close();
           _logger.debug("Link Closed");
           link = con.linkHead(EnumSet.of(EndpointState.ACTIVE),
                   EnumSet.of(EndpointState.CLOSED));
       }

       // teardown any terminating sessions
       ssn = con.sessionHead(EnumSet.of(EndpointState.ACTIVE),
               EnumSet.of(EndpointState.CLOSED));
       while (ssn != null)
       {
           ssn.close();
           _logger.debug("Session Closed");
           ssn = con.sessionHead(EnumSet.of(EndpointState.ACTIVE),
                   EnumSet.of(EndpointState.CLOSED));
       }

       // teardown the connection if it's terminating
       if (con.getRemoteState() == EndpointState.CLOSED)
       {
           _logger.debug("Connection Closed");
           con.close();
       }
    }

    private void setupLink(Link link)
    {
        String src = link.getRemoteSourceAddress();
        String target = link.getRemoteTargetAddress();

        if (link instanceof Sender)
        {
            _logger.debug("Opening Link to read from mailbox: " + src);
            if (!_mailboxes.containsKey(src))
            {
                _logger.error("Error: mailbox " + src + " does not exist!");
                // TODO report error.
            }
        }
        else
        {
            _logger.debug("Opening Link to write from mailbox: " + target);
            if (!_mailboxes.containsKey(target))
            {
                _mailboxes.put(target, new ArrayList<byte[]>());
            }
        }

        link.setLocalSourceAddress(src);
        link.setLocalTargetAddress(target);

        if (link instanceof Sender)
        {
            // grant a delivery to the link - it will become "writable" when the
            // driver can accept messages for the sender.
            String id = "server-delivery-" + _counter;
            link.delivery(id.getBytes(),0,id.getBytes().length);
            _counter++;
        }
        else
        {
            // Grant enough credit to the receiver to allow one inbound message
            ((Receiver)link).flow(1);
        }
        link.open();
    }

    private void processReceive(Delivery d)
    {
        Receiver rec = (Receiver)d.getLink();
        String mailboxName = rec.getRemoteTargetAddress();
        List<byte[]> mailbox;
        if (!_mailboxes.containsKey(mailboxName))
        {
            _logger.error("Error: cannot sent to mailbox " + mailboxName + " - dropping message.");
        }
        else
        {
            mailbox = _mailboxes.get(mailboxName);
            byte[] readBuf = new byte[1024];
            int  bytesRead = rec.recv(readBuf, 0, readBuf.length);
            ByteArrayOutputStream bout = new ByteArrayOutputStream();
            while (bytesRead > 0)
            {
                bout.write(readBuf, 0, bytesRead);
                bytesRead = rec.recv(readBuf, 0, readBuf.length);
            }
            mailbox.add(bout.toByteArray());
        }

        d.disposition(null);
        d.settle();
        rec.advance();
        if (rec.getCredit() == 0)
        {
            rec.flow(1);
        }
    }

    private void sendMessage(Delivery d)
    {
        Sender sender = (Sender)d.getLink();
        String mailboxName = sender.getRemoteSourceAddress();
        _logger.error("Request for Mailbox : " + mailboxName);
        byte[] msg;
        if (_mailboxes.containsKey(mailboxName))
        {
            msg = _mailboxes.get(mailboxName).remove(0);
            _logger.debug("Fetching message " + new String(msg));
        }
        else
        {
            _logger.debug("Warning: mailbox " + mailboxName + " is empty, sending empty message.");
            msg = "".getBytes();
        }
        sender.send(msg, 0, msg.length);
        if (sender.advance())
        {
            String id = "server-delivery-" + _counter;
            sender.delivery(id.getBytes(),0,id.getBytes().length);
            _counter++;
        }
    }

    public static void main(String[] args) throws Exception
    {
        MailServer server = new MailServer();
        while (true)
        {
            server.doWait();
            server.acceptConnections();
            server.processConnections();
        }
    }

    class Logger implements LogHandler
    {
        static final String prefix = "TestServer: ";

        @Override
        public boolean isTraceEnabled()
        {
            return true;
        }

        @Override
        public void trace(String message)
        {
            System.out.println(prefix + message);
        }

        @Override
        public boolean isDebugEnabled()
        {
            return true;
        }

        @Override
        public void debug(String message)
        {
            System.out.println(prefix + "DEBUG : " + message);
        }

        @Override
        public void debug(Throwable t, String message)
        {
            System.out.println(prefix + "DEBUG : " + message);
            t.printStackTrace();
        }

        @Override
        public boolean isInfoEnabled()
        {
            return true;
        }

        @Override
        public void info(String message)
        {
            System.out.println(prefix + "INFO : " + message);
        }

        @Override
        public void info(Throwable t, String message)
        {
            System.out.println(prefix + "INFO : " + message);
            t.printStackTrace();
        }

        @Override
        public void warn(String message)
        {
            System.out.println(prefix + "WARN : " + message);
        }

        @Override
        public void warn(Throwable t, String message)
        {
            System.out.println(prefix + "INFO : " + message);
            t.printStackTrace();
        }

        @Override
        public void error(String message)
        {
            System.out.println(prefix + "ERROR : " + message);
        }

        @Override
        public void error(Throwable t, String message)
        {
            System.out.println(prefix + "INFO : " + message);
            t.printStackTrace();
        }

    }
}
