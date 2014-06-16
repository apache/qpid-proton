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
import java.nio.ByteBuffer;
import java.nio.channels.SelectionKey;
import java.nio.channels.Selector;
import java.util.ArrayList;
import java.util.EnumSet;
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.apache.qpid.proton.InterruptException;
import org.apache.qpid.proton.Proton;
import org.apache.qpid.proton.TimeoutException;
import org.apache.qpid.proton.amqp.messaging.Source;
import org.apache.qpid.proton.amqp.messaging.Target;
import org.apache.qpid.proton.amqp.transport.ReceiverSettleMode;
import org.apache.qpid.proton.amqp.transport.SenderSettleMode;
import org.apache.qpid.proton.engine.Collector;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.Event;
import org.apache.qpid.proton.engine.Link;
import org.apache.qpid.proton.engine.Receiver;
import org.apache.qpid.proton.engine.Sasl;
import org.apache.qpid.proton.engine.Sender;
import org.apache.qpid.proton.engine.Session;
import org.apache.qpid.proton.engine.Ssl;
import org.apache.qpid.proton.engine.SslDomain;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.engine.TransportException;
import org.apache.qpid.proton.message.Message;
import org.apache.qpid.proton.messenger.Listener;
import org.apache.qpid.proton.messenger.Messenger;
import org.apache.qpid.proton.messenger.MessengerException;
import org.apache.qpid.proton.messenger.Selectable;
import org.apache.qpid.proton.messenger.Status;
import org.apache.qpid.proton.messenger.Tracker;

public class MessengerImpl implements Messenger

{
    private enum LinkCreditMode
    {
        // method for replenishing credit
        LINK_CREDIT_EXPLICIT,   // recv(N)
        LINK_CREDIT_AUTO;       // recv()
    }

    private static final EnumSet<EndpointState> UNINIT = EnumSet.of(EndpointState.UNINITIALIZED);
    private static final EnumSet<EndpointState> ACTIVE = EnumSet.of(EndpointState.ACTIVE);
    private static final EnumSet<EndpointState> CLOSED = EnumSet.of(EndpointState.CLOSED);
    private static final EnumSet<EndpointState> ANY = EnumSet.allOf(EndpointState.class);

    private final Logger _logger = Logger.getLogger("proton.messenger");
    private final String _name;
    private long _timeout = -1;
    private boolean _blocking = true;
    private long _nextTag = 1;
    private LinkCreditMode _credit_mode = LinkCreditMode.LINK_CREDIT_EXPLICIT;
    private final int _credit_batch = 1024;   // credit_mode == LINK_CREDIT_AUTO
    private int _credit;        // available
    private int _distributed;    // outstanding credit
    private int _receivers;      // total # receiver Links
    private int _draining;       // # Links in drain state
    private List<Receiver> _credited = new ArrayList<Receiver>();
    private List<Receiver> _blocked = new ArrayList<Receiver>();
    private long _next_drain;
    private TrackerImpl _incomingTracker;
    private TrackerImpl _outgoingTracker;
    private Store _incomingStore = new Store();
    private Store _outgoingStore = new Store();
    private int _sendThreshold;

    private Transform _routes = new Transform();
    private Transform _rewrites = new Transform();

    private String _certificate;
    private String _privateKey;
    private String _password;
    private String _trustedDb;

    private Collector _collector = Proton.collector();
    private boolean _passive = false;
    private final List<SelectableImpl> _selectables = new ArrayList<SelectableImpl>();
    private final List<ListenerImpl> _listeners = new ArrayList<ListenerImpl>();
    private Selector _selector;
    private AtomicBoolean _closed = new AtomicBoolean(true);
    private AtomicBoolean _interrupted = new AtomicBoolean(false);
    
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
        try
        {
            _selector = Selector.open();
        }
        catch (IOException e)
        {
            throw new RuntimeException("Unable to create selector",e);
        }
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

    public void setCertificate(String certificate)
    {
        _certificate = certificate;
    }

    public String getCertificate()
    {
        return _certificate;
    }

    public void setPrivateKey(String privateKey)
    {
        _privateKey = privateKey;
    }

    public String getPrivateKey()
    {
        return _privateKey;
    }

    public void setPassword(String password)
    {
        _password = password;
    }

    public String getPassword()
    {
        return _password;
    }

    public void setTrustedCertificates(String trusted)
    {
        _trustedDb = trusted;
    }

    public String getTrustedCertificates()
    {
        return _trustedDb;
    }

    public void start() throws IOException
    {
        _closed.set(false); 
    }

    public void stop()
    {
        if (_closed.get())
        {
            return;
        }

        _closed.set(true);
        if(_logger.isLoggable(Level.FINE))
        {
            _logger.fine(this + " about to stop");
        }

        //stop listeners
        for (ListenerImpl l : _listeners)
        {
            l.close();
            if (!_passive)
            {
                try
                {
                    l.getSocketListener().close();
                    l.markCompleted();
                }
                catch (IOException e)
                {
                    _logger.log(Level.SEVERE, String.format("Error closing socket listener : %s", e), e);
                }
            }
        }        

        //close all connections.
        for (SelectableImpl sel : _selectables)
        {
            SelectableImpl s = (SelectableImpl)sel;
            Connection connection = s.getConnection();
            connection.close();
            if (!_passive && !s.getNetworkConnection().isClosed())
            {
                s.getNetworkConnection().registerForWriteEvents(true);
            }
            s.markClosed();
        }

        waitUntil(_allClosed);
        _listeners.clear();
        _selectables.clear();
    }

    public boolean stopped()
    {
        return _allClosed.test();
    }

    public boolean work(long timeout) throws TimeoutException
    {
        if (_closed.get()) { return false; }
        _worked = false;
        return waitUntil(_workPred, timeout);
    }

    public void interrupt()
    {
        _interrupted.set(true);
        if (!_passive)
        {
            _selector.wakeup();
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
        if (_closed.get()) {
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

        Sender sender = getLink(address, new SenderFinder(address.getName()));
        pumpOut(m.getAddress(), sender);
    }

    private void reclaimLink(Link link)
    {
        if (link instanceof Receiver)
        {
            int credit = link.getCredit();
            if (credit > 0)
            {
                _credit += credit;
                _distributed -= credit;
            }
        }

        Delivery delivery = link.head();
        while (delivery != null)
        {
            StoreEntry entry = (StoreEntry) delivery.getContext();
            if (entry != null)
            {
                entry.setDelivery(null);
                if (delivery.isBuffered()) {
                    entry.setStatus(Status.ABORTED);
                }
            }
            delivery = delivery.next();
        }
        linkRemoved(link);
    }

    private int pumpOut( String address, Sender sender )
    {
        StoreEntry entry = _outgoingStore.get( address );
        if (entry == null) {
            sender.drained();
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
        if (_closed.get()) {
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
        if (_closed.get()) {
            throw new IllegalStateException("cannot recv while messenger is stopped");
        }

        if (_logger.isLoggable(Level.FINE) && n != -1)
        {
            _logger.fine(this + " about to wait for up to " + n + " messages to be received");
        }

        if (n == -1)
        {
            _credit_mode = LinkCreditMode.LINK_CREDIT_AUTO;
        }
        else
        {
            _credit_mode = LinkCreditMode.LINK_CREDIT_EXPLICIT;
            if (n > _distributed)
                _credit = n - _distributed;
            else        // cancel unallocated
                _credit = 0;
        }

        distributeCredit();

        waitUntil(_messageAvailable);
    }

    public void recv() throws TimeoutException
    {
        recv(-1);
    }

    public int receiving()
    {
        return _credit + _distributed;
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

            // account for the used credit, replenish if
            // low (< 20% maximum per-link batch) and
            // extra credit available
            assert(_distributed > 0);
            _distributed--;
            if (!receiver.getDrain() && _blocked.isEmpty() && _credit > 0)
            {
                final int max = perLinkCredit();
                final int lo_thresh = (int)(max * 0.2 + 0.5);
                if (receiver.getRemoteCredit() < lo_thresh)
                {
                    final int more = Math.min(_credit, max - receiver.getRemoteCredit());
                    _credit -= more;
                    _distributed += more;
                    receiver.flow(more);
                }
            }
            // check if blocked
            if (receiver.getRemoteCredit() == 0 && _credited.contains(receiver))
            {
                _credited.remove(receiver);
                if (receiver.getDrain())
                {
                    receiver.setDrain(false);
                    assert( _draining > 0 );
                    _draining--;
                }
                _blocked.add(receiver);
            }
        }
        return 0;
    }

    public void subscribe(String source) throws MessengerException
    {
        if (_closed.get()) {
            throw new IllegalStateException("messenger is stopped");
        }

        String routed = routeAddress(source);
        Address address = new Address(routed);

        String hostName = address.getHost();
        if (hostName == null) throw new MessengerException("Invalid address (hostname cannot be null): " + routed);
        int port = Integer.valueOf(address.getImpliedPort());
        if (address.isPassive())
        {
            if(_logger.isLoggable(Level.FINE))
            {
                _logger.fine(this + " about to subscribe to source " + source + " using address " + hostName + ":" + port);
            }
            ListenerContext ctx = new ListenerContext(address);
            ListenerImpl listener = new ListenerImpl(hostName, port);
            listener.setContext(ctx);
            if (!_passive)
            {
                try
                {
                    SocketListener socket = new SocketListener(_selector, listener, hostName , port);
                    listener.setSocketListener(socket);
                }
                catch (IOException e)
                {
                    _logger.log(Level.SEVERE, String.format("Error subscribing to message source : %s", e), e);
                    throw new MessengerException(String.format("Error subscribing to message source : %s", e),e);
                }
            }
            _listeners.add(listener);
        }
        else
        {
            if(_logger.isLoggable(Level.FINE))
            {
                _logger.fine(this + " about to subscribe to source " + source);
            }
            getLink(address, new ReceiverFinder(address.getName()));
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

    @Override
    public void setPassive(boolean b)
    {
        _passive = b;
    }

    @Override
    public boolean isPassive()
    {
        return _passive;
    }

    @Override
    public List<? extends Selectable> getSelectables()
    {
        return _selectables;
    }

    @Override
    public List<? extends Listener> getListeners()
    {
        // TODO Auto-generated method stub
        return null;
    }

    private int queued(boolean outgoing)
    {
        int count = 0;
        if (!_closed.get()) {
            for (SelectableImpl sel : _selectables)
            {
                Connection connection = sel.getConnection();
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

    private void processEvents()
    {
        for (Event event = _collector.peek(); event != null; event = _collector.peek())
        {
            switch (event.getType())
            {
            case CONNECTION_REMOTE_STATE:
            case CONNECTION_LOCAL_STATE:
                processConnection(event.getConnection());
                break;
            case SESSION_REMOTE_STATE:
            case SESSION_LOCAL_STATE:
                processSession(event.getSession());
                break;
            case LINK_REMOTE_STATE:
            case LINK_LOCAL_STATE:
                processLink(event.getLink());
                break;
            case LINK_FLOW:
                processFlow(event.getLink());
                break;
            case DELIVERY:
                processDelivery(event.getDelivery());
                break;
            case TRANSPORT:
                distributeCredit();
                break;
            }
            _collector.pop();
        }
    }

    private void processConnection(Connection connection)
    {
        if ( connection.getRemoteState() == EndpointState.ACTIVE)
        {
            if (connection.getLocalState() == EndpointState.UNINITIALIZED)
            {
                connection.open();                
            }
        }
        if (connection.getRemoteState() == EndpointState.CLOSED)
        {
            if (connection.getLocalState() == EndpointState.ACTIVE)
            {
                connection.close();
            }
        }
        ConnectionContext ctx = (ConnectionContext)connection.getContext();
        if (!_passive)
        {
            ((SelectableImpl)ctx.getSelectable()).getNetworkConnection().registerForWriteEvents(true);
        }
    }

    private void processSession(Session session)
    {
        if (session.getRemoteState() == EndpointState.ACTIVE)
        {
            if (session.getLocalState() == EndpointState.UNINITIALIZED)
            {
                session.open();
                _logger.log(Level.FINE, "Opened session " + session);
            }
        }
        if (session.getRemoteState() == EndpointState.CLOSED)
        {
            if (session.getLocalState() == EndpointState.ACTIVE)
            {
                session.close();
            }
        }
        ConnectionContext ctx = (ConnectionContext)session.getConnection().getContext();
        if (!_passive)
        {
            ((SelectableImpl)ctx.getSelectable()).getNetworkConnection().registerForWriteEvents(true);
        }
    }

    private void processLink(Link link)
    {
        if (link.getRemoteState() == EndpointState.ACTIVE)
        {
            if (link.getLocalState() == EndpointState.UNINITIALIZED)
            {
                link.setSource(link.getRemoteSource());
                link.setTarget(link.getRemoteTarget());
                linkAdded(link);
                link.open();
                _logger.log(Level.FINE, "Opened link " + link);
            }
        }
        if (link.getRemoteState() == EndpointState.CLOSED)
        {
            if (link.getLocalState() == EndpointState.ACTIVE)
            {
                link.close();
            }
            else
            {
                reclaimLink(link);
            }
        }
        ConnectionContext ctx = (ConnectionContext)link.getSession().getConnection().getContext();
        if (!_passive)
        {
            ((SelectableImpl)ctx.getSelectable()).getNetworkConnection().registerForWriteEvents(true);
        }
    }

    private void processFlow(Link link)
    {
        if (link instanceof Sender)
        {
            pumpOut(link.getTarget().getAddress(), (Sender)link);
        }
        ConnectionContext ctx = (ConnectionContext)link.getSession().getConnection().getContext();
        if (!_passive)
        {
            ((SelectableImpl)ctx.getSelectable()).getNetworkConnection().registerForWriteEvents(true);
        }
    }

    private void processDelivery(Delivery delivery)
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

        delivery.clear();
        ConnectionContext ctx = (ConnectionContext)link.getSession().getConnection().getContext();
        if (!_passive)
        {
            ((SelectableImpl)ctx.getSelectable()).getNetworkConnection().registerForWriteEvents(true);
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
        processEvents();

        if (_passive)
        {
            return condition.test();
        }

        // wait until timeout expires or until test is true
        long now = System.currentTimeMillis();
        final long deadline = timeout < 0 ? Long.MAX_VALUE : now + timeout;
        boolean done = false;

        while (true)
        {
            done = condition.test();
            if (done)
                break;

            long remaining;
            if (timeout < 0)
                remaining = -1;
            else {
                remaining = deadline - now;
                if (remaining < 0) break;
            }

            // Update the credit scheduler. If the scheduler detects
            // credit imbalance on the links, wake up in time to
            // service credit drain
            distributeCredit();
            if (_next_drain != 0)
            {
                long wakeup = (_next_drain > now) ? _next_drain - now : 0;
                remaining = (remaining == -1) ? wakeup : Math.min(remaining, wakeup);
            }
            processPendingSelectables(remaining);
            processEvents();
            if (_interrupted.get())
            {
                _interrupted.set(false);
                throw new InterruptException();
            }
            now = System.currentTimeMillis();            
        }

        return done;
    }

    // Used when passive mode is false.
    private void processPendingSelectables(long timeout)
    {
        try
        {
            if (timeout == 0)
            {
                _selector.selectNow();
            }
            else if (timeout < 0)
            {
                _selector.select();
            }
            else
            {
                _selector.select(timeout);
            }
        }
        catch (IOException e)
        {
            _logger.log(Level.SEVERE, "Exception when waiting for IO Event", e);
            throw new RuntimeException("Exception when waiting for IO Event", e);
        }

        for (SelectionKey key : _selector.selectedKeys())
        {
            if (key.isValid() && key.isAcceptable())
            {
                ListenerImpl listener = (ListenerImpl) key.attachment();
                try
                {
                    inboundConnection(listener, listener.getSocketListener().accept());
                }
                catch (IOException e)
                {
                    _logger.log(Level.SEVERE, "Exception when accepting connection", e);
                    throw new RuntimeException("Exception when accepting connection", e);
                }
            }
            else if (key.isValid() && key.isReadable())
            {
                connectionReadable((SelectableImpl) key.attachment());
            }
            else if (key.isValid() && key.isWritable())
            {
                connectionWritable((SelectableImpl) key.attachment());
            }
        }
        _selector.selectedKeys().clear();

        if (_closed.get() && !_passive)
        {
            for(SelectableImpl sel : _selectables)
            {
                try
                {
                    sel.getNetworkConnection().close();
                }
                catch (IOException e)
                {
                    _logger.log(Level.WARNING, "Error while closing connection", e);
                }
                sel.markCompleted();
            }
        }
    }

    private Connection lookup(Address address)
    {
        for (SelectableImpl sel : _selectables)
        {
            Connection connection = sel.getConnection();
            ConnectionContext ctx = (ConnectionContext) connection.getContext();
            if (ctx.matches(address))
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
            reclaimLink(link);
        }
    }

    private void distributeCredit()
    {
        if (_receivers == 0) return;

        if (_credit_mode == LinkCreditMode.LINK_CREDIT_AUTO)
        {
            // replenish, but limit the max total messages buffered
            final int max = _receivers * _credit_batch;
            final int used = _distributed + incoming();
            if (max > used)
                _credit = max - used;
        }

        // reclaim any credit left over after draining links has completed
        if (_draining > 0)
        {
            Iterator<Receiver> itr = _credited.iterator();
            while (itr.hasNext())
            {
                Receiver link = (Receiver) itr.next();
                if (link.getDrain())
                {
                    if (!link.draining())
                    {
                        // drain completed for this link
                        int drained = link.drained();
                        assert(_distributed >= drained);
                        _distributed -= drained;
                        _credit += drained;
                        link.setDrain(false);
                        _draining--;
                        itr.remove();
                        _blocked.add(link);
                    }
                }
            }
        }

        // distribute available credit to blocked links
        final int batch = perLinkCredit();
        while (_credit > 0 && !_blocked.isEmpty())
        {
            Receiver link = _blocked.get(0);
            _blocked.remove(0);

            final int more = Math.min(_credit, batch);
            _distributed += more;
            _credit -= more;

            link.flow(more);
            _credited.add(link);

            // flow changed, must process it
            ConnectionContext ctx = (ConnectionContext) link.getSession().getConnection().getContext();
            if (!_passive)
            {
                ((SelectableImpl)ctx.getSelectable()).getNetworkConnection().registerForWriteEvents(true);
            }
        }

        if (_blocked.isEmpty())
        {
            _next_drain = 0;
        }
        else
        {
            // not enough credit for all links - start draining granted credit
            if (_draining == 0)
            {
                // don't do it too often - pace ourselves (it's expensive)
                if (_next_drain == 0)
                {
                    _next_drain = System.currentTimeMillis() + 250;
                }
                else if (_next_drain <= System.currentTimeMillis())
                {
                    // initiate drain, free up at most enough to satisfy blocked
                    _next_drain = 0;
                    int needed = _blocked.size() * batch;

                    for (Receiver link : _credited)
                    {
                        if (!link.getDrain()) {
                            link.setDrain(true);
                            needed -= link.getRemoteCredit();
                            _draining++;
                            // drain requested on link, must process it
                            ConnectionContext ctx = (ConnectionContext) link.getSession().getConnection().getContext();
                            if (!_passive)
                            {
                                ((SelectableImpl)ctx.getSelectable()).getNetworkConnection().registerForWriteEvents(true);
                            }
                            if (needed <= 0) break;
                        }
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

            for (SelectableImpl sel : _selectables)
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
                Connection connection = sel.getConnection();
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
            for (SelectableImpl sel : _selectables)
            {
                Connection connection = sel.getConnection();
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
            if (_closed.get() ||( _selectables.size() == 0 && _listeners.size() == 0))
                return true;

            return false;
        }
    }

    private class AllClosed implements Predicate
    {
        public boolean test()
        {
            for (Listener l : _listeners)
            {
                if (!l.isCompleted())
                {
                    return false;
                }
            }
            for (Selectable sel : _selectables)
            {
                if (!sel.isCompleted())
                {
                    return false;
                }
            }
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

    private <C extends Link> C getLink(Address address, LinkFinder<C> finder)
    {
        Connection connection = lookup(address);
        if (connection == null)
        {
            String host = address.getHost();
            int port = Integer.valueOf(address.getImpliedPort());
            SelectableImpl sel = new SelectableImpl(host, port);

            connection = Proton.connection();
            connection.collect(_collector);
            connection.setContainer(_name);
            connection.setHostname(host);
            connection.setContext(new ConnectionContext(address, sel));
            Transport transport = Proton.transport();

            if (!_passive)
            {
                IoConnection networkConnection = new IoConnection(_selector, host, port);
                _logger.log(Level.FINE, "Connecting to " + host + ":" + port);
                networkConnection.setSelectable(sel);
                sel.setNetworkConnection(networkConnection);
                sel.markConnected();
                networkConnection.registerForReadEvents(true);
                networkConnection.registerForWriteEvents(true);
            }
            sel.setTransport(transport);
            sel.setConnection(connection);
            _selectables.add(sel);
            transport.bind(connection);
            Sasl sasl = transport.sasl();
            if (sasl != null)
            {
                sasl.client();
                sasl.setMechanisms(new String[]{"ANONYMOUS"});
            }
            if ("amqps".equalsIgnoreCase(address.getScheme())) {
                SslDomain domain = makeDomain(address, SslDomain.Mode.CLIENT);
                if (_trustedDb != null) {
                    domain.setPeerAuthentication(SslDomain.VerifyMode.VERIFY_PEER);
                    //domain.setPeerAuthentication(SslDomain.VerifyMode.VERIFY_PEER_NAME);
                } else {
                    domain.setPeerAuthentication(SslDomain.VerifyMode.ANONYMOUS_PEER);
                }
                Ssl ssl = transport.ssl(domain);
                //ssl.setPeerHostname(host);
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
        linkAdded(link);
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

    @Override
    public String toString()
    {
        StringBuilder builder = new StringBuilder();
        builder.append("MessengerImpl [_name=").append(_name).append("]");
        return builder.toString();
    }

    // compute the maximum amount of credit each receiving link is
    // entitled to.  The actual credit given to the link depends on
    // what amount of credit is actually available.
    private int perLinkCredit()
    {
        if (_receivers == 0) return 0;
        int total = _credit + _distributed;
        return Math.max(total/_receivers, 1);
    }

    // a new link has been created, account for it.
    private void linkAdded(Link link)
    {
        if (link instanceof Receiver)
        {
            _receivers++;
            _blocked.add((Receiver)link);
        }
    }

    // a link is being removed, account for it.
    private void linkRemoved(Link _link)
    {
        if (_link instanceof Receiver)
        {
            Receiver link = (Receiver)_link;
            assert _receivers > 0;
            _receivers--;
            if (link.getDrain())
            {
                link.setDrain(false);
                assert _draining > 0;
                _draining--;
            }
            if (_blocked.contains(link))
                _blocked.remove(link);
            else if (_credited.contains(link))
                _credited.remove(link);
            else
                assert(false);
        }
    }

    private class ConnectionContext
    {
        private Address _address;
        private SelectableImpl _selectable;

        public ConnectionContext(Address address, SelectableImpl selectable)
        {
            _address = address;
            _selectable = selectable;
        }

        public Address getAddress()
        {
            return _address;
        }

        public boolean matches(Address address)
        {
            String host = address.getHost();
            String port = address.getImpliedPort();
            return host.equals(_selectable.getConnection().getRemoteContainer()) ||
                (_address.getHost().equals(host) && _address.getImpliedPort().equals(port));
        }

        public SelectableImpl getSelectable()
        {
            return _selectable;
        }
    }

    private SslDomain makeDomain(Address address, SslDomain.Mode mode)
    {
        SslDomain domain = Proton.sslDomain();
        domain.init(mode);
        if (_certificate != null) {
            domain.setCredentials(_certificate, _privateKey, _password);
        }
        if (_trustedDb != null) {
            domain.setTrustedCaDb(_trustedDb);
        }

        if ("amqps".equalsIgnoreCase(address.getScheme())) {
            domain.allowUnsecuredClient(false);
        } else {
            domain.allowUnsecuredClient(true);
        }

        return domain;
    }


    private class ListenerContext
    {
        private Address _address;
        private SslDomain _domain;

        public ListenerContext(Address address)
        {
            _address = address;
            _domain = makeDomain(address, SslDomain.Mode.SERVER);
        }

        public SslDomain getDomain()
        {
            return _domain;
        }

        public Address getAddress()
        {
            return _address;
        }

    }

    /* ----------------------------------------
     * IO events for non passive mode
     * ----------------------------------------
     */
    void inboundConnection(Listener listener, IoConnection networkConnection)
    {
        //System.out.println("inboundConnection ............................................"+ _name);

        _worked = true;
        Connection connection = Proton.connection();
        connection.collect(_collector);
        connection.setContainer(_name);
        
        Transport transport = Proton.transport();
        transport.bind(connection);
        SelectableImpl selectable = new SelectableImpl();
        selectable.markConnected();
        selectable.setTransport(transport);
        selectable.setConnection(connection);
        selectable.setNetworkConnection(networkConnection);
        _selectables.add(selectable);
        
        ListenerContext ctx = (ListenerContext) ((ListenerImpl)listener).getContext();
        connection.setContext(new ConnectionContext(ctx.getAddress(), selectable));
        networkConnection.setSelectable(selectable);
        
        //TODO: full SASL
        Sasl sasl = transport.sasl();
        if (sasl != null)
        {
            sasl.server();
            sasl.setMechanisms(new String[]{"ANONYMOUS"});
            sasl.done(Sasl.SaslOutcome.PN_SASL_OK);
        }
        transport.ssl(ctx.getDomain());
        connection.open();
        networkConnection.registerForReadEvents(true);
        networkConnection.registerForWriteEvents(true);
    }
    
    void connectionReadable(SelectableImpl selectable)
    {
        //System.out.println("connectionReadable ............................................" + _name);
        
        _worked = true;
        IoConnection networkConnection = selectable.getNetworkConnection();
        SelectableImpl sel = (SelectableImpl) selectable;
        Transport transport = sel.getTransport();
        ByteBuffer tail = transport.tail();
        try
        {
            int read = networkConnection.read(tail);
            if (read < 0)
            {
                connectionClosed(sel);
                return;
            }
        }
        catch (IOException e)
        {
            _logger.log(Level.SEVERE, this + " error reading from file descriptor", e);
            // Need to throw the exception as well.
        }
        try
        {
            transport.process();
        }
        catch (TransportException e)
        {
            _logger.log(Level.SEVERE, this + " error processing input", e);
        }
        networkConnection.registerForReadEvents(transport.capacity() > 0);
        networkConnection.registerForWriteEvents(true);
    }

   void connectionWritable(SelectableImpl selectable)
    {
       //System.out.println("connectionWritable ............................................" + _name);
       
        _worked = true;
        IoConnection networkConnection = selectable.getNetworkConnection();
        SelectableImpl sel = (SelectableImpl) selectable;
        Transport transport = sel.getTransport();
        boolean writeBlocked = false;
        try
        {
            while (transport.pending() > 0 && !writeBlocked)
            {
                ByteBuffer head = transport.head();
                int wrote = 0;
                try
                {
                    wrote = networkConnection.write(head);
                }
                catch (IOException e)
                {
                    _logger.log(Level.SEVERE, this + " error writing to the file descriptor", e);
                    // Need to throw the exception as well.
                }
                if (wrote > 0)
                {
                    transport.pop(wrote);
                }
                else
                {
                    writeBlocked = true;                    
                }
            }
            networkConnection.registerForWriteEvents(transport.pending() > 0);

            if (selectable.isClosed() && !_passive)
            {
                try
                {
                    selectable.getNetworkConnection().close();
                }
                catch (IOException e)
                {
                    _logger.log(Level.SEVERE, String.format("Error closing connection : %s", e), e);
                }
                selectable.markCompleted();
            }
            
        }
        catch (TransportException e)
        {
            _logger.log(Level.SEVERE, this + " error", e);
            networkConnection.registerForWriteEvents(false);
        }
    }

    void connectionClosed(SelectableImpl sel)
    {
        reclaimCredit(sel.getConnection());
        sel.markClosed();
        if (!_passive)
        {
            try
            {
                sel.getNetworkConnection().close();
            }
            catch (IOException e)
            {
                _logger.log(Level.SEVERE, String.format("Error closing connection : %s", e), e);
            }
        }
        sel.markCompleted();
        _selectables.remove(sel);
    }
}