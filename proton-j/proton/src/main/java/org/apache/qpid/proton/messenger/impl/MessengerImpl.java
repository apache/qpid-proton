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
package org.apache.qpid.proton.messenger.impl;

import java.io.IOException;
import java.util.ArrayList;
import java.util.EnumSet;
import java.util.Iterator;
import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.apache.qpid.proton.Proton;
import org.apache.qpid.proton.ProtonFactoryLoader;
import org.apache.qpid.proton.InterruptException;
import org.apache.qpid.proton.TimeoutException;
import org.apache.qpid.proton.driver.Connector;
import org.apache.qpid.proton.driver.Driver;
import org.apache.qpid.proton.driver.DriverFactory;
import org.apache.qpid.proton.driver.Listener;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.EngineFactory;
import org.apache.qpid.proton.engine.Link;
import org.apache.qpid.proton.engine.Receiver;
import org.apache.qpid.proton.engine.Sasl;
import org.apache.qpid.proton.engine.Sender;
import org.apache.qpid.proton.engine.Session;
import org.apache.qpid.proton.message.Message;
import org.apache.qpid.proton.message.MessageFactory;
import org.apache.qpid.proton.messenger.Messenger;
import org.apache.qpid.proton.messenger.MessengerException;
import org.apache.qpid.proton.messenger.MessengerFactory;
import org.apache.qpid.proton.messenger.Status;
import org.apache.qpid.proton.messenger.Tracker;
import org.apache.qpid.proton.amqp.messaging.Source;
import org.apache.qpid.proton.amqp.messaging.Target;
import org.apache.qpid.proton.amqp.transport.ReceiverSettleMode;
import org.apache.qpid.proton.amqp.transport.SenderSettleMode;

import org.apache.qpid.proton.amqp.Binary;

public class MessengerImpl implements Messenger
{

    private static final EnumSet<EndpointState> UNINIT = EnumSet.of(EndpointState.UNINITIALIZED);
    private static final EnumSet<EndpointState> ACTIVE = EnumSet.of(EndpointState.ACTIVE);
    private static final EnumSet<EndpointState> CLOSED = EnumSet.of(EndpointState.CLOSED);
    private static final EnumSet<EndpointState> ANY = EnumSet.allOf(EndpointState.class);

    private final Logger _logger = Logger.getLogger("proton.messenger");
    private final String _name;
    private long _timeout = -1;
    private boolean _blocking = true;
    private long _nextTag = 1;
    private Driver _driver;
    private int _receiving = 0;
    private static final int _creditBatch = 1024;
    private int _credit;
    private int _distributed;
    private TrackerImpl _incomingTracker;
    private TrackerImpl _outgoingTracker;
    private Store _incomingStore = new Store();
    private Store _outgoingStore = new Store();
    private List<Connector> _awaitingDestruction = new ArrayList<Connector>();
    private int _sendThreshold;

    private Transform _routes = new Transform();
    private Transform _rewrites = new Transform();


    /**
     * @deprecated This constructor's visibility will be reduced to the default scope in a future release.
     * Client code outside this module should use a {@link MessengerFactory} instead
     */
    @Deprecated public MessengerImpl()
    {
        this(java.util.UUID.randomUUID().toString());
    }

    /**
     * @deprecated This constructor's visibility will be reduced to the default scope in a future release.
     * Client code outside this module should use a {@link MessengerFactory} instead
     */
    @Deprecated public MessengerImpl(String name)
    {
        _name = name;
    }

    public void setTimeout(long timeInMillis)
    {
        _timeout = timeInMillis;
    }

    public long getTimeout()
    {
        return _timeout;
    }

    public boolean isBlocking()
    {
        return _blocking;
    }

    public void setBlocking(boolean b)
    {
        _blocking = b;
    }

    public void start() throws IOException
    {
        _driver = Proton.driver();
    }

    public void stop()
    {
        if (_driver != null) {
            if(_logger.isLoggable(Level.FINE))
            {
                _logger.fine(this + " about to stop");
            }
            //close all connections
            for (Connector<?> c : _driver.connectors())
            {
                Connection connection = c.getConnection();
                connection.close();
            }
            //stop listeners
            for (Listener<?> l : _driver.listeners())
            {
                try
                {
                    l.close();
                }
                catch (IOException e)
                {
                    _logger.log(Level.WARNING, "Error while closing listener", e);
                }
            }
            waitUntil(_allClosed);
        }
    }

    public boolean stopped()
    {
        return _allClosed.test();
    }

    public boolean work(long timeout)
    {
        if (_driver == null) { return false; }
        _worked = false;
        return waitUntil(_workPred, timeout);
    }

    public void interrupt()
    {
        if (_driver != null) {
            _driver.wakeup();
        }
    }

    private String defaultRewrite(String address) {
        if (address != null && address.contains("@")) {
            Address addr = new Address(address);
            String scheme = addr.getScheme();
            String host = addr.getHost();
            String port = addr.getPort();
            String name = addr.getName();

            StringBuilder sb = new StringBuilder();
            if (scheme != null) {
                sb.append(scheme).append("://");
            }
            if (host != null) {
                sb.append(host);
            }
            if (port != null) {
                sb.append(":").append(port);
            }
            if (name != null) {
                sb.append("/").append(name);
            }
            return sb.toString();
        } else {
            return address;
        }
    }


    private String _original;

    private void rewriteMessage(Message m)
    {
        _original = m.getAddress();
        if (_rewrites.apply(_original)) {
            m.setAddress(_rewrites.result());
        } else {
            m.setAddress(defaultRewrite(_original));
        }
    }

    private void restoreMessage(Message m)
    {
        m.setAddress(_original);
    }

    private String routeAddress(String addr)
    {
        if (_routes.apply(addr)) {
            return _routes.result();
        } else {
            return addr;
        }
    }

    public void put(Message m) throws MessengerException
    {
        if (_driver == null) {
            throw new IllegalStateException("cannot put while messenger is stopped");
        }

        if(_logger.isLoggable(Level.FINE))
        {
            _logger.fine(this + " about to put message: " + m);
        }

        StoreEntry entry = _outgoingStore.put( m.getAddress() );
        _outgoingTracker = new TrackerImpl(TrackerImpl.Type.OUTGOING,
                                           _outgoingStore.trackEntry(entry));

        String routedAddress = routeAddress(m.getAddress());
        Address address = new Address(routedAddress);
        if (address.getHost() == null)
        {
            throw new MessengerException("unable to send to address: " + routedAddress);
        }

        rewriteMessage(m);

        try {
            adjustReplyTo(m);

            int encoded;
            byte[] buffer = new byte[5*1024];
            while (true)
            {
                try
                {
                    encoded = m.encode(buffer, 0, buffer.length);
                    break;
                } catch (java.nio.BufferOverflowException e) {
                    buffer = new byte[buffer.length*2];
                }
            }
            entry.setEncodedMsg( buffer, encoded );
        }
        finally
        {
            restoreMessage(m);
        }

        String ports = address.getPort() == null ? defaultPort(address.getScheme()) : address.getPort();
        int port = Integer.valueOf(ports);
        Sender sender = getLink(address.getHost(), port, new SenderFinder(address.getName()));
        pumpOut(m.getAddress(), sender);
    }

    private int pumpOut( String address, Sender sender )
    {
        StoreEntry entry = _outgoingStore.get( address );
        if (entry == null) {
            return 0;
        }

        byte[] tag = String.valueOf(_nextTag++).getBytes();
        Delivery delivery = sender.delivery(tag);
        entry.setDelivery( delivery );
        _logger.log(Level.FINE, "Sending on delivery: " + delivery);
        int n = sender.send( entry.getEncodedMsg(), 0, entry.getEncodedLength());
        if (n < 0) {
            _outgoingStore.freeEntry( entry );
            _logger.log(Level.WARNING, "Send error: " + n);
            return n;
        } else {
            sender.advance();
            _outgoingStore.freeEntry( entry );
            return 0;
        }
    }

    public void send() throws TimeoutException
    {
        send(-1);
    }

    public void send(int n) throws TimeoutException
    {
        if (_driver == null) {
            throw new IllegalStateException("cannot send while messenger is stopped");
        }

        if(_logger.isLoggable(Level.FINE))
        {
            _logger.fine(this + " about to send");
        }

        if (n == -1)
            _sendThreshold = 0;
        else
        {
            _sendThreshold = outgoing() - n;
            if (_sendThreshold < 0)
                _sendThreshold = 0;
        }

        waitUntil(_sentSettled);
    }

    public void recv(int n) throws TimeoutException
    {
        if (_driver == null) {
            throw new IllegalStateException("cannot recv while messenger is stopped");
        }

        if(_logger.isLoggable(Level.FINE))
        {
            _logger.fine(this + " about to wait for up to " + n + " messages to be received");
        }

        _receiving = n;
        distributeCredit();

        waitUntil(_messageAvailable);
    }

    public void recv() throws TimeoutException
    {
        recv(-1);
    }

    public int receiving()
    {
        return _receiving;
    }

    public Message get()
    {
        StoreEntry entry = _incomingStore.get( null );
        if (entry != null)
        {
            Message message = Proton.message();
            message.decode( entry.getEncodedMsg(), 0, entry.getEncodedLength() );

            _incomingTracker = new TrackerImpl(TrackerImpl.Type.INCOMING,
                                               _incomingStore.trackEntry(entry));

            _incomingStore.freeEntry( entry );
            _distributed--;
            return message;
        }

        return null;
    }

    private int pumpIn(String address, Receiver receiver)
    {
        Delivery delivery = receiver.current();
        if (delivery.isReadable() && !delivery.isPartial())
        {
            StoreEntry entry = _incomingStore.put( address );
            entry.setDelivery( delivery );

            _logger.log(Level.FINE, "Readable delivery found: " + delivery);

            int size = delivery.pending();
            byte[] buffer = new byte[size];
            int read = receiver.recv( buffer, 0, buffer.length );
            if (read != size) {
                throw new IllegalStateException();
            }
            entry.setEncodedMsg( buffer, size );
            receiver.advance();
        }
        return 0;
    }

    public void subscribe(String source) throws MessengerException
    {
        if (_driver == null) {
            throw new IllegalStateException("messenger is stopped");
        }

        String routed = routeAddress(source);
        Address address = new Address(routed);

        String hostName = address.getHost();
        if (hostName == null) throw new MessengerException("Invalid address (hostname cannot be null): " + routed);
        String ports = address.getPort() == null ? defaultPort(address.getScheme()) : address.getPort();
        int port = Integer.valueOf(ports);
        if (address.isPassive())
        {
            if(_logger.isLoggable(Level.FINE))
            {
                _logger.fine(this + " about to subscribe to source " + source + " using address " + hostName + ":" + port);
            }
            _driver.createListener(hostName, port, null);
        }
        else
        {
            if(_logger.isLoggable(Level.FINE))
            {
                _logger.fine(this + " about to subscribe to source " + source);
            }
            getLink(hostName, port, new ReceiverFinder(address.getName()));
        }
    }

    public int outgoing()
    {
        return _outgoingStore.size() + queued(true);
    }

    public int incoming()
    {
        return _incomingStore.size() + queued(false);
    }

    public int getIncomingWindow()
    {
        return _incomingStore.getWindow();
    }

    public void setIncomingWindow(int window)
    {
        _incomingStore.setWindow(window);
    }

    public int getOutgoingWindow()
    {
        return _outgoingStore.getWindow();
    }

    public void setOutgoingWindow(int window)
    {
        _outgoingStore.setWindow(window);
    }

    public Tracker incomingTracker()
    {
        return _incomingTracker;
    }
    public Tracker outgoingTracker()
    {
        return _outgoingTracker;
    }

    private Store getTrackerStore(Tracker tracker)
    {
        return ((TrackerImpl)tracker).isOutgoing() ? _outgoingStore : _incomingStore;
    }

    @Override
    public void reject(Tracker tracker, int flags)
    {
        int id = ((TrackerImpl)tracker).getSequence();
        getTrackerStore(tracker).update(id, Status.REJECTED, flags, false, false);
    }

    @Override
    public void accept(Tracker tracker, int flags)
    {
        int id = ((TrackerImpl)tracker).getSequence();
        getTrackerStore(tracker).update(id, Status.ACCEPTED, flags, false, false);
    }

    @Override
    public void settle(Tracker tracker, int flags)
    {
        int id = ((TrackerImpl)tracker).getSequence();
        getTrackerStore(tracker).update(id, Status.UNKNOWN, flags, true, true);
    }

    public Status getStatus(Tracker tracker)
    {
        int id = ((TrackerImpl)tracker).getSequence();
        StoreEntry e = getTrackerStore(tracker).getEntry(id);
        if (e != null)
        {
            return e.getStatus();
        }
        return Status.UNKNOWN;
    }

    @Override
    public void route(String pattern, String address)
    {
        _routes.rule(pattern, address);
    }

    @Override
    public void rewrite(String pattern, String address)
    {
        _rewrites.rule(pattern, address);
    }

    private int queued(boolean outgoing)
    {
        int count = 0;
        if (_driver != null) {
            for (Connector<?> c : _driver.connectors())
            {
                Connection connection = c.getConnection();
                for (Link link : new Links(connection, ACTIVE, ANY))
                {
                    if (outgoing)
                    {
                        if (link instanceof Sender) count += link.getQueued();
                    }
                    else
                    {
                        if (link instanceof Receiver) count += link.getQueued();
                    }
                }
            }
        }
        return count;
    }

    private void bringDestruction()
    {
        for (Connector<?> c : _awaitingDestruction)
        {
            c.destroy();
        }
        _awaitingDestruction.clear();
    }

    private void processAllConnectors()
    {
        distributeCredit();
        for (Connector<?> c : _driver.connectors())
        {
            processEndpoints(c);
            try
            {
                if (c.process()) {
                    _worked = true;
                }
            }
            catch (IOException e)
            {
                _logger.log(Level.SEVERE, "Error processing connection", e);
            }
        }
        bringDestruction();
        distributeCredit();
    }

    private void processActive()
    {
        //process active listeners
        for (Listener<?> l = _driver.listener(); l != null; l = _driver.listener())
        {
            _worked = true;
            Connector<?> c = l.accept();
            Connection connection = Proton.connection();
            connection.setContainer(_name);
            c.setConnection(connection);
            //TODO: SSL and full SASL
            Sasl sasl = c.sasl();
            if (sasl != null)
            {
                sasl.server();
                sasl.setMechanisms(new String[]{"ANONYMOUS"});
                sasl.done(Sasl.SaslOutcome.PN_SASL_OK);
            }
            connection.open();
        }
        //process active connectors, handling opened & closed connections as needed
        for (Connector<?> c = _driver.connector(); c != null; c = _driver.connector())
        {
            _worked = true;
            _logger.log(Level.FINE, "Processing active connector " + c);
            try
            {
                c.process();
            } catch (IOException e) {
                _logger.log(Level.SEVERE, "Error processing connection", e);
            }
            processEndpoints(c);
            if (c.isClosed())
            {
                _awaitingDestruction.add(c);
                reclaimCredit(c.getConnection());
            }
            else
            {
                try
                {
                    c.process();
                }
                catch (IOException e)
                {
                    _logger.log(Level.SEVERE, "Error processing connection", e);
                }
            }
        }
        bringDestruction();
        distributeCredit();
    }

    private void processEndpoints(Connector c)
    {
        Connection connection = c.getConnection();

        if (connection.getLocalState() == EndpointState.UNINITIALIZED)
        {
            connection.open();
        }

        Delivery delivery = connection.getWorkHead();
        while (delivery != null)
        {
            Link link = delivery.getLink();
            if (delivery.isUpdated())
            {
                if (link instanceof Sender)
                {
                    delivery.disposition(delivery.getRemoteState());
                }
                StoreEntry e = (StoreEntry) delivery.getContext();
                if (e != null) e.updated();
            }

            if (delivery.isReadable())
            {
                pumpIn( link.getSource().getAddress(), (Receiver)link );
            }

            Delivery next = delivery.getWorkNext();
            delivery.clear();
            delivery = next;
        }

        for (Session session : new Sessions(connection, UNINIT, ANY))
        {
            session.open();
            _logger.log(Level.FINE, "Opened session " + session);
        }
        for (Link link : new Links(connection, UNINIT, ANY))
        {
            //TODO: the following is not correct; should only copy those properties that we understand
            link.setSource(link.getRemoteSource());
            link.setTarget(link.getRemoteTarget());
            link.open();
            _logger.log(Level.FINE, "Opened link " + link);
        }

        distributeCredit();

        for (Link link : new Links(connection, ACTIVE, ACTIVE))
        {
            if (link instanceof Sender)
            {
                pumpOut(link.getTarget().getAddress(), (Sender)link);
            }
        }

        for (Link link : new Links(connection, ACTIVE, CLOSED))
        {
            link.close();
        }
        for (Session session : new Sessions(connection, ACTIVE, CLOSED))
        {
            session.close();
        }
        if (connection.getRemoteState() == EndpointState.CLOSED)
        {
            if (connection.getLocalState() == EndpointState.ACTIVE)
            {
                connection.close();
            }
        }
    }

    private boolean waitUntil(Predicate condition) throws TimeoutException
    {
        if (_blocking) {
            boolean done = waitUntil(condition, _timeout);
            if (!done) {
                _logger.log(Level.SEVERE, String.format
                            ("Timeout when waiting for condition %s after %s ms",
                             condition, _timeout));
                throw new TimeoutException();
            }
            return done;
        } else {
            return waitUntil(condition, 0);
        }
    }

    private boolean waitUntil(Predicate condition, long timeout)
    {
        if (_driver == null) {
            throw new IllegalStateException("cannot wait while messenger is stopped");
        }

        processAllConnectors();

        // wait until timeout expires or until test is true
        long now = System.currentTimeMillis();
        long deadline = timeout < 0 ? Long.MAX_VALUE : now + timeout;
        boolean done = false;

        while (true)
        {
            done = condition.test();
            if (done) break;

            boolean woken;
            if ( timeout >= 0 ) {
                long remaining = deadline - now;
                if (remaining < 0) break;
                woken = _driver.doWait(remaining);
            } else {
                woken = _driver.doWait(-1);
            }
            processActive();
            if (woken) {
                throw new InterruptException();
            }
            if (timeout >= 0) {
                now = System.currentTimeMillis();
            }
        }

        return done;
    }

    private Connection lookup(String host, String service)
    {
        for (Connector<?> c : _driver.connectors())
        {
            Connection connection = c.getConnection();
            if (host.equals(connection.getRemoteContainer()) || service.equals(connection.getContext()))
            {
                return connection;
            }
        }
        return null;
    }

    private void reclaimCredit(Connection connection)
    {
        for (Link link : new Links(connection, ANY, ANY))
        {
            if (link instanceof Receiver && link.getCredit() > 0)
            {
                reclaimCredit(link.getCredit());
            }
        }
    }

    private void reclaimCredit(int credit)
    {
        _credit += credit;
        _distributed -= credit;
    }

    private void distributeCredit()
    {
        int linkCt = 0;
        // @todo track the number of opened receive links
        for (Connector<?> c : _driver.connectors())
        {
            if (c.isClosed()) continue;
            Connection connection = c.getConnection();
            for (Link link : new Links(connection, ACTIVE, ANY))
            {
                if (link instanceof Receiver) linkCt++;
            }
        }

        if (linkCt == 0) return;

        if (_receiving < 0)
        {
            _credit = linkCt * _creditBatch - incoming();
        } else {
            int total = _credit + _distributed;
            if (_receiving > total)
                _credit += _receiving - total;
        }

        int batch = (_credit < linkCt) ? 1 : (_credit/linkCt);
        for (Connector<?> c : _driver.connectors())
        {
            if (c.isClosed()) continue;
            Connection connection = c.getConnection();
            for (Link link : new Links(connection, ACTIVE, ANY))
            {
                if (link instanceof Receiver)
                {
                    int have = ((Receiver) link).getCredit();
                    if (have < batch)
                    {
                        int need = batch - have;
                        int amount = (_credit < need) ? _credit : need;
                        ((Receiver) link).flow(amount);
                        _credit -= amount;
                        _distributed += amount;
                        if (_credit == 0) return;
                    }
                }
            }
        }
    }

    private interface Predicate
    {
        boolean test();
    }

    private class SentSettled implements Predicate
    {
        public boolean test()
        {
            //are all sent messages settled?
            int total = _outgoingStore.size();

            for (Connector<?> c : _driver.connectors())
            {
                // TBD
                // check if transport is done generating output
                // pn_transport_t *transport = pn_connector_transport(ctor);
                // if (transport) {
                //    if (!pn_transport_quiesced(transport)) {
                //        pn_connector_process(ctor);
                //        return false;
                //    }
                // }

                Connection connection = c.getConnection();
                for (Link link : new Links(connection, ACTIVE, ANY))
                {
                    if (link instanceof Sender)
                    {
                        total += link.getQueued();
                    }
                }

                // TBD: there is no per-link unsettled
                // deliveries iterator, so for now get the
                // deliveries by walking the outgoing trackers
                Iterator<StoreEntry> entries = _outgoingStore.trackedEntries();
                while (entries.hasNext() && total <= _sendThreshold)
                {
                    StoreEntry e = (StoreEntry) entries.next();
                    if (e != null )
                    {
                        Delivery d = e.getDelivery();
                        if (d != null)
                        {
                            if (d.getRemoteState() == null && !d.remotelySettled())
                            {
                                total++;
                            }
                        }
                    }
                }
            }
            return total <= _sendThreshold;
        }
    }

    private class MessageAvailable implements Predicate
    {
        public boolean test()
        {
            //do we have at least one pending message?
            if (_incomingStore.size() > 0) return true;
            for (Connector<?> c : _driver.connectors())
            {
                Connection connection = c.getConnection();
                Delivery delivery = connection.getWorkHead();
                while (delivery != null)
                {
                    if (delivery.isReadable() && !delivery.isPartial())
                    {
                        return true;
                    }
                    else
                    {
                        delivery = delivery.getWorkNext();
                    }
                }
            }
            // if no connections, or not listening, exit as there won't ever be a message
            if (!_driver.listeners().iterator().hasNext() && !_driver.connectors().iterator().hasNext())
                return true;

            return false;
        }
    }

    private class AllClosed implements Predicate
    {
        public boolean test()
        {
            if (_driver == null) {
                return true;
            }

            for (Connector<?> c : _driver.connectors()) {
                if (!c.isClosed()) {
                    return false;
                }
            }

            _driver.destroy();
            _driver = null;

            return true;
        }
    }

    private boolean _worked = false;

    private class WorkPred implements Predicate
    {
        public boolean test()
        {
            return _worked;
        }
    }

    private final SentSettled _sentSettled = new SentSettled();
    private final MessageAvailable _messageAvailable = new MessageAvailable();
    private final AllClosed _allClosed = new AllClosed();
    private final WorkPred _workPred = new WorkPred();

    private interface LinkFinder<C extends Link>
    {
        C test(Link link);
        C create(Session session);
    }

    private class SenderFinder implements LinkFinder<Sender>
    {
        private final String _path;

        SenderFinder(String path)
        {
            _path = path == null ? "" : path;
        }

        public Sender test(Link link)
        {
            if (link instanceof Sender && matchTarget((Target) link.getTarget(), _path))
            {
                return (Sender) link;
            }
            else
            {
                return null;
            }
        }

        public Sender create(Session session)
        {
            Sender sender = session.sender(_path);
            Target target = new Target();
            target.setAddress(_path);
            sender.setTarget(target);
            // the C implemenation does this:
            Source source = new Source();
            source.setAddress(_path);
            sender.setSource(source);
            if (getOutgoingWindow() > 0)
            {
                // use explicit settlement via dispositions (not pre-settled)
                sender.setSenderSettleMode(SenderSettleMode.UNSETTLED);
                sender.setReceiverSettleMode(ReceiverSettleMode.SECOND);  // desired
            }
            return sender;
        }
    }

    private class ReceiverFinder implements LinkFinder<Receiver>
    {
        private final String _path;

        ReceiverFinder(String path)
        {
            _path = path == null ? "" : path;
        }

        public Receiver test(Link link)
        {
            if (link instanceof Receiver && matchSource((Source) link.getSource(), _path))
            {
                return (Receiver) link;
            }
            else
            {
                return null;
            }
        }

        public Receiver create(Session session)
        {
            Receiver receiver = session.receiver(_path);
            Source source = new Source();
            source.setAddress(_path);
            receiver.setSource(source);
            // the C implemenation does this:
            Target target = new Target();
            target.setAddress(_path);
            receiver.setTarget(target);
            if (getIncomingWindow() > 0)
            {
                // use explicit settlement via dispositions (not pre-settled)
                receiver.setSenderSettleMode(SenderSettleMode.UNSETTLED);  // desired
                receiver.setReceiverSettleMode(ReceiverSettleMode.SECOND);
            }
            return receiver;
        }
    }

    private <C extends Link> C getLink(String host, int port, LinkFinder<C> finder)
    {
        String service = host + ":" + port;
        Connection connection = lookup(host, service);
        if (connection == null)
        {
            Connector<?> connector = _driver.createConnector(host, port, null);
            _logger.log(Level.FINE, "Connecting to " + host + ":" + port);
            connection = Proton.connection();
            connection.setContainer(_name);
            connection.setHostname(host);
            connection.setContext(service);
            connector.setConnection(connection);
            Sasl sasl = connector.sasl();
            if (sasl != null)
            {
                sasl.client();
                sasl.setMechanisms(new String[]{"ANONYMOUS"});
            }
            connection.open();
        }

        for (Link link : new Links(connection, ACTIVE, ANY))
        {
            C result = finder.test(link);
            if (result != null) return result;
        }
        Session session = connection.session();
        session.open();
        C link = finder.create(session);
        link.open();
        return link;
    }

    private static class Links implements Iterable<Link>
    {
        private final Connection _connection;
        private final EnumSet<EndpointState> _local;
        private final EnumSet<EndpointState> _remote;

        Links(Connection connection, EnumSet<EndpointState> local, EnumSet<EndpointState> remote)
        {
            _connection = connection;
            _local = local;
            _remote = remote;
        }

        public java.util.Iterator<Link> iterator()
        {
            return new LinkIterator(_connection, _local, _remote);
        }
    }

    private static class LinkIterator implements java.util.Iterator<Link>
    {
        private final EnumSet<EndpointState> _local;
        private final EnumSet<EndpointState> _remote;
        private Link _next;

        LinkIterator(Connection connection, EnumSet<EndpointState> local, EnumSet<EndpointState> remote)
        {
            _local = local;
            _remote = remote;
            _next = connection.linkHead(_local, _remote);
        }

        public boolean hasNext()
        {
            return _next != null;
        }

        public Link next()
        {
            try
            {
                return _next;
            }
            finally
            {
                _next = _next.next(_local, _remote);
            }
        }

        public void remove()
        {
            throw new UnsupportedOperationException();
        }
    }

    private static class Sessions implements Iterable<Session>
    {
        private final Connection _connection;
        private final EnumSet<EndpointState> _local;
        private final EnumSet<EndpointState> _remote;

        Sessions(Connection connection, EnumSet<EndpointState> local, EnumSet<EndpointState> remote)
        {
            _connection = connection;
            _local = local;
            _remote = remote;
        }

        public java.util.Iterator<Session> iterator()
        {
            return new SessionIterator(_connection, _local, _remote);
        }
    }

    private static class SessionIterator implements java.util.Iterator<Session>
    {
        private final EnumSet<EndpointState> _local;
        private final EnumSet<EndpointState> _remote;
        private Session _next;

        SessionIterator(Connection connection, EnumSet<EndpointState> local, EnumSet<EndpointState> remote)
        {
            _local = local;
            _remote = remote;
            _next = connection.sessionHead(_local, _remote);
        }

        public boolean hasNext()
        {
            return _next != null;
        }

        public Session next()
        {
            try
            {
                return _next;
            }
            finally
            {
                _next = _next.next(_local, _remote);
            }
        }

        public void remove()
        {
            throw new UnsupportedOperationException();
        }
    }

    private void adjustReplyTo(Message m)
    {
        String original = m.getReplyTo();
        if (original != null) {
            if (original.startsWith("~/"))
            {
                m.setReplyTo("amqp://" + _name + "/" + original.substring(2));
            }
            else if (original.equals("~"))
            {
                m.setReplyTo("amqp://" + _name);
            }
        }
    }

    private static boolean matchTarget(Target target, String path)
    {
        if (target == null) return path.isEmpty();
        else return path.equals(target.getAddress());
    }

    private static boolean matchSource(Source source, String path)
    {
        if (source == null) return path.isEmpty();
        else return path.equals(source.getAddress());
    }

    private static String defaultPort(String scheme)
    {
        if ("amqps".equals(scheme)) return "5671";
        else return "5672";
    }

    @Override
    public String toString()
    {
        StringBuilder builder = new StringBuilder();
        builder.append("MessengerImpl [_name=").append(_name).append("]");
        return builder.toString();
    }

}
