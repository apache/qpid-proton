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

package org.apache.qpid.proton.engine.impl;

import static org.apache.qpid.proton.engine.impl.ByteBufferUtils.pourArrayToBuffer;
import static org.apache.qpid.proton.engine.impl.ByteBufferUtils.pourBufferToArray;

import java.nio.ByteBuffer;
import java.util.HashMap;
import java.util.Map;

import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.amqp.UnsignedShort;
import org.apache.qpid.proton.amqp.transport.Attach;
import org.apache.qpid.proton.amqp.transport.Begin;
import org.apache.qpid.proton.amqp.transport.Close;
import org.apache.qpid.proton.amqp.transport.ConnectionError;
import org.apache.qpid.proton.amqp.transport.DeliveryState;
import org.apache.qpid.proton.amqp.transport.Detach;
import org.apache.qpid.proton.amqp.transport.Disposition;
import org.apache.qpid.proton.amqp.transport.End;
import org.apache.qpid.proton.amqp.transport.ErrorCondition;
import org.apache.qpid.proton.amqp.transport.Flow;
import org.apache.qpid.proton.amqp.transport.FrameBody;
import org.apache.qpid.proton.amqp.transport.Open;
import org.apache.qpid.proton.amqp.transport.Role;
import org.apache.qpid.proton.amqp.transport.Transfer;
import org.apache.qpid.proton.codec.AMQPDefinedTypes;
import org.apache.qpid.proton.codec.DecoderImpl;
import org.apache.qpid.proton.codec.EncoderImpl;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.Event;
import org.apache.qpid.proton.engine.ProtonJTransport;
import org.apache.qpid.proton.engine.Sasl;
import org.apache.qpid.proton.engine.Ssl;
import org.apache.qpid.proton.engine.SslDomain;
import org.apache.qpid.proton.engine.SslPeerDetails;
import org.apache.qpid.proton.engine.TransportException;
import org.apache.qpid.proton.engine.TransportResult;
import org.apache.qpid.proton.engine.TransportResultFactory;
import org.apache.qpid.proton.engine.WebSocket;
import org.apache.qpid.proton.engine.impl.ssl.SslImpl;
import org.apache.qpid.proton.framing.TransportFrame;
import org.apache.qpid.proton.reactor.Reactor;
import org.apache.qpid.proton.reactor.Selectable;

public class TransportImpl extends EndpointImpl
    implements ProtonJTransport, FrameBody.FrameBodyHandler<Integer>,
        FrameHandler, TransportOutputWriter
{
    static final int BUFFER_RELEASE_THRESHOLD = Integer.getInteger("proton.transport_buffer_release_threshold", 2 * 1024 * 1024);
    private static final int CHANNEL_MAX_LIMIT = 65535;

    private static final boolean getBooleanEnv(String name)
    {
        String value = System.getenv(name);
        return "true".equalsIgnoreCase(value) ||
            "1".equals(value) ||
            "yes".equalsIgnoreCase(value);
    }

    private static final boolean FRM_ENABLED = getBooleanEnv("PN_TRACE_FRM");
    private static final int TRACE_FRAME_PAYLOAD_LENGTH = Integer.getInteger("proton.trace_frame_payload_length", 1024);

    // trace levels
    private int _levels = (FRM_ENABLED ? TRACE_FRM : 0);

    private FrameParser _frameParser;

    private ConnectionImpl _connectionEndpoint;

    private boolean _isOpenSent;
    private boolean _isCloseSent;

    private boolean _headerWritten;
    private Map<Integer, TransportSession> _remoteSessions = new HashMap<Integer, TransportSession>();
    private Map<Integer, TransportSession> _localSessions = new HashMap<Integer, TransportSession>();

    private TransportInput _inputProcessor;
    private TransportOutput _outputProcessor;

    private DecoderImpl _decoder = new DecoderImpl();
    private EncoderImpl _encoder = new EncoderImpl(_decoder);

    private int _maxFrameSize = DEFAULT_MAX_FRAME_SIZE;
    private int _remoteMaxFrameSize = 512;
    private int _channelMax       = CHANNEL_MAX_LIMIT;
    private int _remoteChannelMax = CHANNEL_MAX_LIMIT;

    private final FrameWriter _frameWriter;

    private boolean _closeReceived;
    private Open _open;
    private WebSocketImpl _webSocketImpl;
    private SaslImpl _sasl;
    private SslImpl _ssl;
    private final Ref<ProtocolTracer> _protocolTracer = new Ref(null);

    private TransportResult _lastTransportResult = TransportResultFactory.ok();

    private boolean _init;
    private boolean _processingStarted;
    private boolean _emitFlowEventOnSend = true;

    private FrameHandler _frameHandler = this;
    private boolean _head_closed = false;
    private ErrorCondition _condition = null;

    private boolean postedHeadClosed = false;
    private boolean postedTailClosed = false;
    private boolean postedTransportError = false;

    private int _localIdleTimeout = 0;
    private int _remoteIdleTimeout = 0;
    private long _bytesInput = 0;
    private long _bytesOutput = 0;
    private long _localIdleDeadline = 0;
    private long _lastBytesInput = 0;
    private long _lastBytesOutput = 0;
    private long _remoteIdleDeadline = 0;

    private Selectable _selectable;
    private Reactor _reactor;

    /**
     * @deprecated This constructor's visibility will be reduced to the default scope in a future release.
     * Client code outside this module should use {@link org.apache.qpid.proton.engine.Transport.Factory#create()} instead
     */
    @Deprecated public TransportImpl()
    {
        this(DEFAULT_MAX_FRAME_SIZE);
    }


    /**
     * Creates a transport with the given maximum frame size.
     * Note that the maximumFrameSize also determines the size of the output buffer.
     */
    TransportImpl(int maxFrameSize)
    {
        AMQPDefinedTypes.registerAllTypes(_decoder, _encoder);

        _maxFrameSize = maxFrameSize;
        _frameWriter = new FrameWriter(_encoder, _remoteMaxFrameSize,
                                       FrameWriter.AMQP_FRAME_TYPE,
                                       _protocolTracer,
                                       this);
    }

    private void init()
    {
        if(!_init)
        {
            _init = true;
            _frameParser = new FrameParser(_frameHandler , _decoder, _maxFrameSize);
            _inputProcessor = _frameParser;
            _outputProcessor = new TransportOutputAdaptor(this, _maxFrameSize);
        }
    }

    @Override
    public void trace(int levels) {
        _levels = levels;
    }

    @Override
    public int getMaxFrameSize()
    {
        return _maxFrameSize;
    }

    @Override
    public int getRemoteMaxFrameSize()
    {
        return _remoteMaxFrameSize;
    }

    @Override
    public void setMaxFrameSize(int maxFrameSize)
    {
        if(_init)
        {
            throw new IllegalStateException("Cannot set max frame size after transport has been initialised");
        }
        _maxFrameSize = maxFrameSize;
    }

    @Override
    public int getChannelMax()
    {
        return _channelMax;
    }

    @Override
    public void setChannelMax(int channelMax)
    {
        if(_isOpenSent)
        {
          throw new IllegalArgumentException("Cannot change channel max after open frame has been sent");
        }

        if(channelMax < 0 || channelMax >= (1<<16))
        {
            throw new NumberFormatException("Value \""+channelMax+"\" lies outside the range [0-" + (1<<16) +").");
        }

        _channelMax = channelMax;
    }

    @Override
    public int getRemoteChannelMax()
    {
        return _remoteChannelMax;
    }

    @Override
    public ErrorCondition getCondition()
    {
        return _condition;
    }

    @Override
    public void bind(Connection conn)
    {
        // TODO - check if already bound

        _connectionEndpoint = (ConnectionImpl) conn;
        put(Event.Type.CONNECTION_BOUND, conn);
        _connectionEndpoint.setTransport(this);
        _connectionEndpoint.incref();

        if(getRemoteState() != EndpointState.UNINITIALIZED)
        {
            _connectionEndpoint.handleOpen(_open);
            if(getRemoteState() == EndpointState.CLOSED)
            {
                _connectionEndpoint.setRemoteState(EndpointState.CLOSED);
            }

            _frameParser.flush();
        }
    }

    @Override
    public void unbind()
    {
        for (TransportSession ts: _localSessions.values()) {
            ts.unbind();
        }
        for (TransportSession ts: _remoteSessions.values()) {
            ts.unbind();
        }

        put(Event.Type.CONNECTION_UNBOUND, _connectionEndpoint);

        _connectionEndpoint.modifyEndpoints();
        _connectionEndpoint.setTransport(null);
        _connectionEndpoint.decref();
    }

    @Override
    public int input(byte[] bytes, int offset, int length)
    {
        oldApiCheckStateBeforeInput(length).checkIsOk();

        ByteBuffer inputBuffer = getInputBuffer();
        int numberOfBytesConsumed = pourArrayToBuffer(bytes, offset, length, inputBuffer);
        processInput().checkIsOk();
        return numberOfBytesConsumed;
    }

    /**
     * This method is public as it is used by Python layer.
     * @see org.apache.qpid.proton.engine.Transport#input(byte[], int, int)
     */
    public TransportResult oldApiCheckStateBeforeInput(int inputLength)
    {
        _lastTransportResult.checkIsOk();
        if(inputLength == 0)
        {
            if(_connectionEndpoint == null || _connectionEndpoint.getRemoteState() != EndpointState.CLOSED)
            {
                return TransportResultFactory.error(new TransportException("Unexpected EOS when remote connection not closed: connection aborted"));
            }
        }
        return TransportResultFactory.ok();
    }

    //==================================================================================================================
    // Process model state to generate output

    @Override
    public int output(byte[] bytes, final int offset, final int size)
    {
        ByteBuffer outputBuffer = getOutputBuffer();
        int numberOfBytesOutput = pourBufferToArray(outputBuffer, bytes, offset, size);
        outputConsumed();
        return numberOfBytesOutput;
    }

    @Override
    public boolean writeInto(ByteBuffer outputBuffer)
    {
        processHeader();
        processOpen();
        processBegin();
        processAttach();
        processReceiverFlow();
        // we process transport work twice intentionally, the first
        // pass may end up settling deliveries that the second pass
        // can clean up
        processTransportWork();
        processTransportWork();
        processSenderFlow();
        processDetach();
        processEnd();
        processClose();

        _frameWriter.readBytes(outputBuffer);

        return _isCloseSent || _head_closed;
    }

    @Override
    public Sasl sasl()
    {
        if(_sasl == null)
        {
            if(_processingStarted)
            {
                throw new IllegalStateException("Sasl can't be initiated after transport has started processing");
            }

            init();
            _sasl = new SaslImpl(this, _remoteMaxFrameSize);
            TransportWrapper transportWrapper = _sasl.wrap(_inputProcessor, _outputProcessor);
            _inputProcessor = transportWrapper;
            _outputProcessor = transportWrapper;
        }
        return _sasl;

    }

    @Override
    public WebSocket webSocket()
    {
        if (_webSocketImpl == null)
        {
            init();
            _webSocketImpl = new WebSocketImpl(_remoteMaxFrameSize);
            TransportWrapper transportWrapper = _webSocketImpl.wrap(_inputProcessor, _outputProcessor);
            _inputProcessor = transportWrapper;
            _outputProcessor = transportWrapper;
        }
        return _webSocketImpl;
    }

    /**
     * {@inheritDoc}
     *
     * <p>Note that sslDomain must implement {@link org.apache.qpid.proton.engine.impl.ssl.ProtonSslEngineProvider}.
     * This is not possible enforce at the API level because {@link org.apache.qpid.proton.engine.impl.ssl.ProtonSslEngineProvider} is not part of the
     * public Proton API.</p>
     */
    @Override
    public Ssl ssl(SslDomain sslDomain, SslPeerDetails sslPeerDetails)
    {
        if (_ssl == null)
        {
            init();
            _ssl = new SslImpl(sslDomain, sslPeerDetails);
            TransportWrapper transportWrapper = _ssl.wrap(_inputProcessor, _outputProcessor);
            _inputProcessor = transportWrapper;
            _outputProcessor = transportWrapper;
        }
        return _ssl;
    }

    @Override
    public Ssl ssl(SslDomain sslDomain)
    {
        return ssl(sslDomain, null);
    }

    private void processDetach()
    {
        if(_connectionEndpoint != null && _isOpenSent)
        {
            EndpointImpl endpoint = _connectionEndpoint.getTransportHead();
            while(endpoint != null)
            {

                if(endpoint instanceof LinkImpl)
                {
                    LinkImpl link = (LinkImpl) endpoint;
                    TransportLink<?> transportLink = getTransportState(link);
                    SessionImpl session = link.getSession();
                    TransportSession transportSession = getTransportState(session);

                    if(((link.getLocalState() == EndpointState.CLOSED) || link.detached())
                       && transportLink.isLocalHandleSet()
                       && transportSession.isLocalChannelSet()
                       && !_isCloseSent)
                    {
                        if((link instanceof SenderImpl)
                           && link.getQueued() > 0
                           && !transportLink.detachReceived()
                           && !transportSession.endReceived()
                           && !_closeReceived) {
                            endpoint = endpoint.transportNext();
                            continue;
                        }

                        UnsignedInteger localHandle = transportLink.getLocalHandle();
                        transportLink.clearLocalHandle();
                        transportSession.freeLocalHandle(localHandle);


                        Detach detach = new Detach();
                        detach.setHandle(localHandle);
                        detach.setClosed(!link.detached());

                        ErrorCondition localError = link.getCondition();
                        if( localError.getCondition() !=null )
                        {
                            detach.setError(localError);
                        }


                        writeFrame(transportSession.getLocalChannel(), detach, null, null);
                    }

                    endpoint.clearModified();

                }
                endpoint = endpoint.transportNext();
            }
        }
    }

    private void writeFlow(TransportSession ssn, TransportLink link)
    {
        Flow flow = new Flow();
        flow.setNextIncomingId(ssn.getNextIncomingId());
        flow.setNextOutgoingId(ssn.getNextOutgoingId());
        ssn.updateIncomingWindow();
        flow.setIncomingWindow(ssn.getIncomingWindowSize());
        flow.setOutgoingWindow(ssn.getOutgoingWindowSize());
        if (link != null) {
            flow.setHandle(link.getLocalHandle());
            flow.setDeliveryCount(link.getDeliveryCount());
            flow.setLinkCredit(link.getLinkCredit());
            flow.setDrain(link.getLink().getDrain());
        }
        writeFrame(ssn.getLocalChannel(), flow, null, null);
    }

    private void processSenderFlow()
    {
        if(_connectionEndpoint != null && _isOpenSent && !_isCloseSent)
        {
            EndpointImpl endpoint = _connectionEndpoint.getTransportHead();
            while(endpoint != null)
            {

                if(endpoint instanceof SenderImpl)
                {
                    SenderImpl sender = (SenderImpl) endpoint;
                    if(sender.getDrain() && sender.getDrained() > 0)
                    {
                        TransportSender transportLink = sender.getTransportLink();
                        TransportSession transportSession = sender.getSession().getTransportSession();
                        UnsignedInteger credits = transportLink.getLinkCredit();
                        transportLink.setLinkCredit(UnsignedInteger.valueOf(0));
                        transportLink.setDeliveryCount(transportLink.getDeliveryCount().add(credits));
                        transportLink.setLinkCredit(UnsignedInteger.ZERO);
                        sender.setDrained(0);

                        writeFlow(transportSession, transportLink);
                    }

                }

                endpoint = endpoint.transportNext();
            }
        }
    }

    private void processTransportWork()
    {
        if(_connectionEndpoint != null && _isOpenSent && !_isCloseSent)
        {
            DeliveryImpl delivery = _connectionEndpoint.getTransportWorkHead();
            while(delivery != null)
            {
                LinkImpl link = delivery.getLink();
                if (link instanceof SenderImpl) {
                    if (processTransportWorkSender(delivery, (SenderImpl) link)) {
                        delivery = delivery.clearTransportWork();
                    } else {
                        delivery = delivery.getTransportWorkNext();
                    }
                } else {
                    if (processTransportWorkReceiver(delivery, (ReceiverImpl) link)) {
                        delivery = delivery.clearTransportWork();
                    } else {
                        delivery = delivery.getTransportWorkNext();
                    }
                }
            }
        }
    }

    private boolean processTransportWorkSender(DeliveryImpl delivery,
                                               SenderImpl snd)
    {
        TransportSender tpLink = snd.getTransportLink();
        SessionImpl session = snd.getSession();
        TransportSession tpSession = session.getTransportSession();

        boolean wasDone = delivery.isDone();

        if(!delivery.isDone() &&
           (delivery.getDataLength() > 0 || delivery != snd.current()) &&
           tpSession.hasOutgoingCredit() && tpLink.hasCredit() &&
           tpSession.isLocalChannelSet() &&
           tpLink.getLocalHandle() != null && !_frameWriter.isFull())
        {
            DeliveryImpl inProgress = tpLink.getInProgressDelivery();
            if(inProgress != null){
                // There is an existing Delivery awaiting completion. Check it
                // is the same Delivery object given and return if not, as we
                // can't interleave Transfer frames for deliveries on a link.
                if(inProgress != delivery) {
                    return false;
                }
            }

            UnsignedInteger deliveryId = tpSession.getOutgoingDeliveryId();
            TransportDelivery tpDelivery = new TransportDelivery(deliveryId, delivery, tpLink);
            delivery.setTransportDelivery(tpDelivery);

            final Transfer transfer = new Transfer();
            transfer.setDeliveryId(deliveryId);
            transfer.setDeliveryTag(new Binary(delivery.getTag()));
            transfer.setHandle(tpLink.getLocalHandle());

            if(delivery.getLocalState() != null)
            {
                transfer.setState(delivery.getLocalState());
            }

            if(delivery.isSettled())
            {
                transfer.setSettled(Boolean.TRUE);
            }
            else
            {
                tpSession.addUnsettledOutgoing(deliveryId, delivery);
            }

            if(snd.current() == delivery)
            {
                transfer.setMore(true);
            }

            int messageFormat = delivery.getMessageFormat();
            if(messageFormat == DeliveryImpl.DEFAULT_MESSAGE_FORMAT) {
                transfer.setMessageFormat(UnsignedInteger.ZERO);
            } else {
                transfer.setMessageFormat(UnsignedInteger.valueOf(messageFormat));
            }

            ByteBuffer payload = delivery.getData() ==  null ? null :
                ByteBuffer.wrap(delivery.getData(), delivery.getDataOffset(),
                                delivery.getDataLength());

            writeFrame(tpSession.getLocalChannel(), transfer, payload,
                       new PartialTransfer(transfer));
            tpSession.incrementOutgoingId();
            tpSession.decrementRemoteIncomingWindow();

            if(payload == null || !payload.hasRemaining())
            {
                session.incrementOutgoingBytes(-delivery.pending());
                delivery.setData(null);
                delivery.setDataLength(0);

                if (!transfer.getMore()) {
                    // Clear the in-progress delivery marker
                    tpLink.setInProgressDelivery(null);

                    delivery.setDone();
                    tpLink.setDeliveryCount(tpLink.getDeliveryCount().add(UnsignedInteger.ONE));
                    tpLink.setLinkCredit(tpLink.getLinkCredit().subtract(UnsignedInteger.ONE));
                    tpSession.incrementOutgoingDeliveryId();
                    session.incrementOutgoingDeliveries(-1);
                    snd.decrementQueued();
                }
            }
            else
            {
                int delta = delivery.getDataLength() - payload.remaining();
                delivery.setDataOffset(delivery.getDataOffset() + delta);
                delivery.setDataLength(payload.remaining());
                session.incrementOutgoingBytes(-delta);

                // Remember the delivery we are still processing
                // the body transfer frames for
                tpLink.setInProgressDelivery(delivery);
            }

            if (_emitFlowEventOnSend && snd.getLocalState() != EndpointState.CLOSED) {
                getConnectionImpl().put(Event.Type.LINK_FLOW, snd);
            }
        }

        if(wasDone && delivery.getLocalState() != null)
        {
            TransportDelivery tpDelivery = delivery.getTransportDelivery();
            Disposition disposition = new Disposition();
            disposition.setFirst(tpDelivery.getDeliveryId());
            disposition.setLast(tpDelivery.getDeliveryId());
            disposition.setRole(Role.SENDER);
            disposition.setSettled(delivery.isSettled());
            if(delivery.isSettled())
            {
                tpDelivery.settled();
            }
            disposition.setState(delivery.getLocalState());

            writeFrame(tpSession.getLocalChannel(), disposition, null,
                       null);
        }

        return !delivery.isBuffered();
    }

    private boolean processTransportWorkReceiver(DeliveryImpl delivery,
                                                 ReceiverImpl rcv)
    {
        TransportDelivery tpDelivery = delivery.getTransportDelivery();
        SessionImpl session = rcv.getSession();
        TransportSession tpSession = session.getTransportSession();

        if (tpSession.isLocalChannelSet())
        {
            boolean settled = delivery.isSettled();
            DeliveryState localState = delivery.getLocalState();

            Disposition disposition = new Disposition();
            disposition.setFirst(tpDelivery.getDeliveryId());
            disposition.setLast(tpDelivery.getDeliveryId());
            disposition.setRole(Role.RECEIVER);
            disposition.setSettled(settled);
            disposition.setState(localState);

            if(localState == null && settled) {
                disposition.setState(delivery.getDefaultDeliveryState());
            }

            writeFrame(tpSession.getLocalChannel(), disposition, null, null);
            if (settled)
            {
                tpDelivery.settled();
            }
            return true;
        }

        return false;
    }

    private void processReceiverFlow()
    {
        if(_connectionEndpoint != null && _isOpenSent && !_isCloseSent)
        {
            EndpointImpl endpoint = _connectionEndpoint.getTransportHead();
            while(endpoint != null)
            {
                if(endpoint instanceof ReceiverImpl)
                {
                    ReceiverImpl receiver = (ReceiverImpl) endpoint;
                    TransportLink<?> transportLink = getTransportState(receiver);
                    TransportSession transportSession = getTransportState(receiver.getSession());

                    if(receiver.getLocalState() == EndpointState.ACTIVE)
                    {
                        int credits = receiver.clearUnsentCredits();
                        if(credits != 0 || receiver.getDrain() ||
                           transportSession.getIncomingWindowSize().equals(UnsignedInteger.ZERO))
                        {
                            transportLink.addCredit(credits);
                            writeFlow(transportSession, transportLink);
                        }
                    }
                }
                endpoint = endpoint.transportNext();
            }
            endpoint = _connectionEndpoint.getTransportHead();
            while(endpoint != null)
            {
                if(endpoint instanceof SessionImpl)
                {

                    SessionImpl session = (SessionImpl) endpoint;
                    TransportSession transportSession = getTransportState(session);

                    if(session.getLocalState() == EndpointState.ACTIVE)
                    {
                        if(transportSession.getIncomingWindowSize().equals(UnsignedInteger.ZERO))
                        {
                            writeFlow(transportSession, null);
                        }
                    }
                }
                endpoint = endpoint.transportNext();
            }
        }
    }

    private void processAttach()
    {
        if(_connectionEndpoint != null && _isOpenSent && !_isCloseSent)
        {
            EndpointImpl endpoint = _connectionEndpoint.getTransportHead();

            while(endpoint != null)
            {
                if(endpoint instanceof LinkImpl)
                {

                    LinkImpl link = (LinkImpl) endpoint;
                    TransportLink<?> transportLink = getTransportState(link);
                    if(link.getLocalState() != EndpointState.UNINITIALIZED && !transportLink.attachSent())
                    {

                        if( (link.getRemoteState() == EndpointState.ACTIVE
                            && !transportLink.isLocalHandleSet()) || link.getRemoteState() == EndpointState.UNINITIALIZED)
                        {

                            SessionImpl session = link.getSession();
                            TransportSession transportSession = getTransportState(session);
                            UnsignedInteger localHandle = transportSession.allocateLocalHandle(transportLink);

                            if(link.getRemoteState() == EndpointState.UNINITIALIZED)
                            {
                                transportSession.addHalfOpenLink(transportLink);
                            }

                            Attach attach = new Attach();
                            attach.setHandle(localHandle);
                            attach.setName(transportLink.getName());

                            if(link.getSenderSettleMode() != null)
                            {
                                attach.setSndSettleMode(link.getSenderSettleMode());
                            }

                            if(link.getReceiverSettleMode() != null)
                            {
                                attach.setRcvSettleMode(link.getReceiverSettleMode());
                            }

                            if(link.getSource() != null)
                            {
                                attach.setSource(link.getSource());
                            }

                            if(link.getTarget() != null)
                            {
                                attach.setTarget(link.getTarget());
                            }

                            if(link.getProperties() != null)
                            {
                                attach.setProperties(link.getProperties());
                            }

                            attach.setRole(endpoint instanceof ReceiverImpl ? Role.RECEIVER : Role.SENDER);

                            if(link instanceof SenderImpl)
                            {
                                attach.setInitialDeliveryCount(UnsignedInteger.ZERO);
                            }

                            writeFrame(transportSession.getLocalChannel(), attach, null, null);
                            transportLink.sentAttach();
                        }
                    }
                }
                endpoint = endpoint.transportNext();
            }
        }
    }

    private void processHeader()
    {
        if(!_headerWritten)
        {
            _frameWriter.writeHeader(AmqpHeader.HEADER);
            _headerWritten = true;
        }
    }

    private void processOpen()
    {
        if (!_isOpenSent && (_condition != null ||
             (_connectionEndpoint != null &&
              _connectionEndpoint.getLocalState() != EndpointState.UNINITIALIZED)))
        {
            Open open = new Open();
            if (_connectionEndpoint != null) {
                String cid = _connectionEndpoint.getLocalContainerId();
                open.setContainerId(cid == null ? "" : cid);
                open.setHostname(_connectionEndpoint.getHostname());
                open.setDesiredCapabilities(_connectionEndpoint.getDesiredCapabilities());
                open.setOfferedCapabilities(_connectionEndpoint.getOfferedCapabilities());
                open.setProperties(_connectionEndpoint.getProperties());
            } else {
                open.setContainerId("");
            }

            if (_maxFrameSize > 0) {
                open.setMaxFrameSize(UnsignedInteger.valueOf(_maxFrameSize));
            }
            if (_channelMax > 0) {
                open.setChannelMax(UnsignedShort.valueOf((short) _channelMax));
            }

            // as per the recommendation in the spec, advertise half our
            // actual timeout to the remote
            if (_localIdleTimeout > 0) {
                open.setIdleTimeOut(new UnsignedInteger(_localIdleTimeout / 2));
            }
            _isOpenSent = true;

            writeFrame(0, open, null, null);
        }
    }

    private void processBegin()
    {
        if(_connectionEndpoint != null && _isOpenSent && !_isCloseSent)
        {
            EndpointImpl endpoint = _connectionEndpoint.getTransportHead();
            while(endpoint != null)
            {
                if(endpoint instanceof SessionImpl)
                {
                    SessionImpl session = (SessionImpl) endpoint;
                    TransportSession transportSession = getTransportState(session);
                    if(session.getLocalState() != EndpointState.UNINITIALIZED && !transportSession.beginSent())
                    {
                        int channelId = allocateLocalChannel(transportSession);
                        Begin begin = new Begin();

                        if(session.getRemoteState() != EndpointState.UNINITIALIZED)
                        {
                            begin.setRemoteChannel(UnsignedShort.valueOf((short) transportSession.getRemoteChannel()));
                        }

                        transportSession.updateIncomingWindow();

                        begin.setHandleMax(transportSession.getHandleMax());
                        begin.setIncomingWindow(transportSession.getIncomingWindowSize());
                        begin.setOutgoingWindow(transportSession.getOutgoingWindowSize());
                        begin.setNextOutgoingId(transportSession.getNextOutgoingId());

                        writeFrame(channelId, begin, null, null);
                        transportSession.sentBegin();
                    }
                }
                endpoint = endpoint.transportNext();
            }
        }
    }

    private TransportSession getTransportState(SessionImpl session)
    {
        TransportSession transportSession = session.getTransportSession();
        if(transportSession == null)
        {
            transportSession = new TransportSession(this, session);
            session.setTransportSession(transportSession);
        }
        return transportSession;
    }

    private TransportLink<?> getTransportState(LinkImpl link)
    {
        TransportLink<?> transportLink = link.getTransportLink();
        if(transportLink == null)
        {
            transportLink = TransportLink.createTransportLink(link);
        }
        return transportLink;
    }

    private int allocateLocalChannel(TransportSession transportSession)
    {
        for (int i = 0; i < _connectionEndpoint.getMaxChannels(); i++)
        {
            if (!_localSessions.containsKey(i))
            {
                _localSessions.put(i, transportSession);
                transportSession.setLocalChannel(i);
                return i;
            }
        }

        return -1;
    }

    private int freeLocalChannel(TransportSession transportSession)
    {
        final int channel = transportSession.getLocalChannel();
        _localSessions.remove(channel);
        transportSession.freeLocalChannel();
        return channel;
    }

    private void processEnd()
    {
        if(_connectionEndpoint != null && _isOpenSent)
        {
            EndpointImpl endpoint = _connectionEndpoint.getTransportHead();
            while(endpoint != null)
            {
                SessionImpl session;
                TransportSession transportSession;

                if((endpoint instanceof SessionImpl)) {
                    if ((session = (SessionImpl)endpoint).getLocalState() == EndpointState.CLOSED
                        && (transportSession = session.getTransportSession()).isLocalChannelSet()
                        && !_isCloseSent)
                    {
                        if (hasSendableMessages(session)) {
                            endpoint = endpoint.transportNext();
                            continue;
                        }

                        int channel = freeLocalChannel(transportSession);
                        End end = new End();
                        ErrorCondition localError = endpoint.getCondition();
                        if( localError.getCondition() !=null )
                        {
                            end.setError(localError);
                        }

                        writeFrame(channel, end, null, null);
                    }

                    endpoint.clearModified();
                }

                endpoint = endpoint.transportNext();
            }
        }
    }

    private boolean hasSendableMessages(SessionImpl session)
    {
        if (_connectionEndpoint == null) {
            return false;
        }

        if(!_closeReceived && (session == null || !session.getTransportSession().endReceived()))
        {
            EndpointImpl endpoint = _connectionEndpoint.getTransportHead();
            while(endpoint != null)
            {
                if(endpoint instanceof SenderImpl)
                {
                    SenderImpl sender = (SenderImpl) endpoint;
                    if((session == null || sender.getSession() == session)
                       && sender.getQueued() != 0
                        && !getTransportState(sender).detachReceived())
                    {
                        return true;
                    }
                }
                endpoint = endpoint.transportNext();
            }
        }
        return false;
    }

    private void processClose()
    {
        if ((_condition != null ||
             (_connectionEndpoint != null &&
              _connectionEndpoint.getLocalState() == EndpointState.CLOSED)) &&
            !_isCloseSent) {
            if(!hasSendableMessages(null))
            {
                Close close = new Close();

                ErrorCondition localError;

                if (_connectionEndpoint == null) {
                    localError = _condition;
                } else {
                    localError =  _connectionEndpoint.getCondition();
                }

                if(localError.getCondition() != null)
                {
                    close.setError(localError);
                }

                _isCloseSent = true;

                writeFrame(0, close, null, null);

                if (_connectionEndpoint != null) {
                    _connectionEndpoint.clearModified();
                }
            }
        }
    }

    protected void writeFrame(int channel, FrameBody frameBody,
                            ByteBuffer payload, Runnable onPayloadTooLarge)
    {
        _frameWriter.writeFrame(channel, frameBody, payload, onPayloadTooLarge);
    }

    //==================================================================================================================

    @Override
    protected ConnectionImpl getConnectionImpl()
    {
        return _connectionEndpoint;
    }

    @Override
    void postFinal() {}

    @Override
    void doFree() { }

    //==================================================================================================================
    // handle incoming amqp data


    @Override
    public void handleOpen(Open open, Binary payload, Integer channel)
    {
        setRemoteState(EndpointState.ACTIVE);
        if(_connectionEndpoint != null)
        {
            _connectionEndpoint.handleOpen(open);
        }
        else
        {
            _open = open;
        }

        if(open.getMaxFrameSize().longValue() > 0)
        {
            _remoteMaxFrameSize = (int) open.getMaxFrameSize().longValue();
            _frameWriter.setMaxFrameSize(_remoteMaxFrameSize);
        }

        if (open.getChannelMax().longValue() > 0)
        {
            _remoteChannelMax = (int) open.getChannelMax().longValue();
        }

        if (open.getIdleTimeOut() != null && open.getIdleTimeOut().longValue() > 0)
        {
            _remoteIdleTimeout = open.getIdleTimeOut().intValue();
        }
    }

    @Override
    public void handleBegin(Begin begin, Binary payload, Integer channel)
    {
        // TODO - check channel < max_channel
        TransportSession transportSession = _remoteSessions.get(channel);
        if(transportSession != null)
        {
            // TODO - fail due to begin on begun session
        }
        else
        {
            SessionImpl session;
            if(begin.getRemoteChannel() == null)
            {
                session = _connectionEndpoint.session();
                transportSession = getTransportState(session);
            }
            else
            {
                transportSession = _localSessions.get(begin.getRemoteChannel().intValue());
                if (transportSession == null) {
                    // TODO handle failure rather than just throwing a nicer NPE
                    throw new NullPointerException("uncorrelated channel: " + begin.getRemoteChannel());
                }
                session = transportSession.getSession();

            }
            transportSession.setRemoteChannel(channel);
            session.setRemoteState(EndpointState.ACTIVE);
            transportSession.setNextIncomingId(begin.getNextOutgoingId());
            _remoteSessions.put(channel, transportSession);

            _connectionEndpoint.put(Event.Type.SESSION_REMOTE_OPEN, session);
        }

    }

    @Override
    public void handleAttach(Attach attach, Binary payload, Integer channel)
    {
        TransportSession transportSession = _remoteSessions.get(channel);
        if(transportSession == null)
        {
            // TODO - fail due to attach on non-begun session
        }
        else
        {
            SessionImpl session = transportSession.getSession();
            final UnsignedInteger handle = attach.getHandle();
            if (handle.compareTo(transportSession.getHandleMax()) > 0) {
                // The handle-max value is the highest handle value that can be used on the session. A peer MUST
                // NOT attempt to attach a link using a handle value outside the range that its partner can handle.
                // A peer that receives a handle outside the supported range MUST close the connection with the
                // framing-error error-code.
                ErrorCondition condition =
                        new ErrorCondition(ConnectionError.FRAMING_ERROR,
                                                            "handle-max exceeded");
                _connectionEndpoint.setCondition(condition);
                _connectionEndpoint.setLocalState(EndpointState.CLOSED);
                if (!_isCloseSent) {
                    Close close = new Close();
                    close.setError(condition);
                    _isCloseSent = true;
                    writeFrame(0, close, null, null);
                }
                close_tail();
                return;
            }
            TransportLink<?> transportLink = transportSession.getLinkFromRemoteHandle(handle);
            LinkImpl link = null;

            if(transportLink != null)
            {
                // TODO - fail - attempt attach on a handle which is in use
            }
            else
            {
                transportLink = transportSession.resolveHalfOpenLink(attach.getName());
                if(transportLink == null)
                {

                    link = (attach.getRole() == Role.RECEIVER)
                           ? session.sender(attach.getName())
                           : session.receiver(attach.getName());
                    transportLink = getTransportState(link);
                }
                else
                {
                    link = transportLink.getLink();
                }
                if(attach.getRole() == Role.SENDER)
                {
                    transportLink.setDeliveryCount(attach.getInitialDeliveryCount());
                }

                link.setRemoteState(EndpointState.ACTIVE);
                link.setRemoteSource(attach.getSource());
                link.setRemoteTarget(attach.getTarget());

                link.setRemoteReceiverSettleMode(attach.getRcvSettleMode());
                link.setRemoteSenderSettleMode(attach.getSndSettleMode());

                link.setRemoteProperties(attach.getProperties());

                transportLink.setName(attach.getName());
                transportLink.setRemoteHandle(handle);
                transportSession.addLinkRemoteHandle(transportLink, handle);

            }

            _connectionEndpoint.put(Event.Type.LINK_REMOTE_OPEN, link);
        }
    }

    @Override
    public void handleFlow(Flow flow, Binary payload, Integer channel)
    {
        TransportSession transportSession = _remoteSessions.get(channel);
        if(transportSession == null)
        {
            // TODO - fail due to attach on non-begun session
        }
        else
        {
            transportSession.handleFlow(flow);
        }

    }

    @Override
    public void handleTransfer(Transfer transfer, Binary payload, Integer channel)
    {
        // TODO - check channel < max_channel
        TransportSession transportSession = _remoteSessions.get(channel);
        if(transportSession != null)
        {
            transportSession.handleTransfer(transfer, payload);
        }
        else
        {
            // TODO - fail due to begin on begun session
        }
    }

    @Override
    public void handleDisposition(Disposition disposition, Binary payload, Integer channel)
    {
        TransportSession transportSession = _remoteSessions.get(channel);
        if(transportSession == null)
        {
            // TODO - fail due to attach on non-begun session
        }
        else
        {
            transportSession.handleDisposition(disposition);
        }
    }

    @Override
    public void handleDetach(Detach detach, Binary payload, Integer channel)
    {
        TransportSession transportSession = _remoteSessions.get(channel);
        if(transportSession == null)
        {
            // TODO - fail due to attach on non-begun session
        }
        else
        {
            TransportLink<?> transportLink = transportSession.getLinkFromRemoteHandle(detach.getHandle());

            if(transportLink != null)
            {
                LinkImpl link = transportLink.getLink();
                transportLink.receivedDetach();
                transportSession.freeRemoteHandle(transportLink.getRemoteHandle());
                if (detach.getClosed()) {
                    _connectionEndpoint.put(Event.Type.LINK_REMOTE_CLOSE, link);
                } else {
                    _connectionEndpoint.put(Event.Type.LINK_REMOTE_DETACH, link);
                }
                transportLink.clearRemoteHandle();
                link.setRemoteState(EndpointState.CLOSED);
                if(detach.getError() != null)
                {
                    link.getRemoteCondition().copyFrom(detach.getError());
                }
            }
            else
            {
                // TODO - fail - attempt attach on a handle which is in use
            }
        }
    }

    @Override
    public void handleEnd(End end, Binary payload, Integer channel)
    {
        TransportSession transportSession = _remoteSessions.get(channel);
        if(transportSession == null)
        {
            // TODO - fail due to attach on non-begun session
        }
        else
        {
            _remoteSessions.remove(channel);
            transportSession.receivedEnd();
            transportSession.unsetRemoteChannel();
            SessionImpl session = transportSession.getSession();
            session.setRemoteState(EndpointState.CLOSED);
            ErrorCondition errorCondition = end.getError();
            if(errorCondition != null)
            {
                session.getRemoteCondition().copyFrom(errorCondition);
            }

            _connectionEndpoint.put(Event.Type.SESSION_REMOTE_CLOSE, session);
        }
    }

    @Override
    public void handleClose(Close close, Binary payload, Integer channel)
    {
        _closeReceived = true;
        _remoteIdleTimeout = 0;
        setRemoteState(EndpointState.CLOSED);
        if(_connectionEndpoint != null)
        {
            _connectionEndpoint.setRemoteState(EndpointState.CLOSED);
            if(close.getError() != null)
            {
                _connectionEndpoint.getRemoteCondition().copyFrom(close.getError());
            }

            _connectionEndpoint.put(Event.Type.CONNECTION_REMOTE_CLOSE, _connectionEndpoint);
        }

    }

    @Override
    public boolean handleFrame(TransportFrame frame)
    {
        if (!isHandlingFrames())
        {
            throw new IllegalStateException("Transport cannot accept frame: " + frame);
        }

        log(INCOMING, frame);

        ProtocolTracer tracer = _protocolTracer.get();
        if( tracer != null )
        {
            tracer.receivedFrame(frame);
        }

        frame.getBody().invoke(this,frame.getPayload(), frame.getChannel());
        return _closeReceived;
    }

    void put(Event.Type type, Object context) {
        if (_connectionEndpoint != null) {
            _connectionEndpoint.put(type, context);
        }
    }

    private void maybePostClosed()
    {
        if (postedHeadClosed && postedTailClosed) {
            put(Event.Type.TRANSPORT_CLOSED, this);
        }
    }

    @Override
    public void closed(TransportException error)
    {
        if (!_closeReceived || error != null) {
            if (error == null) {
                _condition = new ErrorCondition(ConnectionError.FRAMING_ERROR,
                                               "connection aborted");
            } else {
                _condition = new ErrorCondition(ConnectionError.FRAMING_ERROR,
                                                error.toString());
            }
            _head_closed = true;
        }
        if (_condition != null && !postedTransportError) {
            put(Event.Type.TRANSPORT_ERROR, this);
            postedTransportError = true;
        }
        if (!postedTailClosed) {
            put(Event.Type.TRANSPORT_TAIL_CLOSED, this);
            postedTailClosed = true;
            maybePostClosed();
        }
    }

    @Override
    public boolean isHandlingFrames()
    {
        return _connectionEndpoint != null || getRemoteState() == EndpointState.UNINITIALIZED;
    }

    @Override
    public ProtocolTracer getProtocolTracer()
    {
        return _protocolTracer.get();
    }

    @Override
    public void setProtocolTracer(ProtocolTracer protocolTracer)
    {
        this._protocolTracer.set(protocolTracer);
    }

    @Override
    public ByteBuffer getInputBuffer()
    {
        return tail();
    }

    @Override
    public TransportResult processInput()
    {
        try {
            process();
            return TransportResultFactory.ok();
        } catch (TransportException e) {
            return TransportResultFactory.error(e);
        }
    }

    @Override
    public ByteBuffer getOutputBuffer()
    {
        pending();
        return head();
    }

    @Override
    public void outputConsumed()
    {
        pop(_outputProcessor.head().position());
    }

    @Override
    public int capacity()
    {
        init();
        return _inputProcessor.capacity();
    }

    @Override
    public ByteBuffer tail()
    {
        init();
        return _inputProcessor.tail();
    }

    @Override
    public void process() throws TransportException
    {
        _processingStarted = true;

        try {
            init();
            int beforePosition = _inputProcessor.position();
            _inputProcessor.process();
            _bytesInput += beforePosition - _inputProcessor.position();
        } catch (TransportException e) {
            _head_closed = true;
            throw e;
        }
    }

    @Override
    public void close_tail()
    {
        init();
        _inputProcessor.close_tail();
    }

    @Override
    public int pending()
    {
        init();
        return _outputProcessor.pending();
    }

    @Override
    public ByteBuffer head()
    {
        init();
        return _outputProcessor.head();
    }

    @Override
    public void pop(int bytes)
    {
        init();
        _outputProcessor.pop(bytes);
        _bytesOutput += bytes;

        int p = pending();
        if (p < 0 && !postedHeadClosed) {
            put(Event.Type.TRANSPORT_HEAD_CLOSED, this);
            postedHeadClosed = true;
            maybePostClosed();
        }
    }

    @Override
    public void setIdleTimeout(int timeout) {
        _localIdleTimeout = timeout;
    }

    @Override
    public int getIdleTimeout() {
        return _localIdleTimeout;
    }

    @Override
    public int getRemoteIdleTimeout() {
        return _remoteIdleTimeout;
    }

    @Override
    public long tick(long now)
    {
        long timeout = 0;

        if (_localIdleTimeout > 0) {
            if (_localIdleDeadline == 0 || _lastBytesInput != _bytesInput) {
                _localIdleDeadline = now + _localIdleTimeout;
                _lastBytesInput = _bytesInput;
            } else if (_localIdleDeadline <= now) {
                _localIdleDeadline = now + _localIdleTimeout;

                if (_connectionEndpoint != null &&
                    _connectionEndpoint.getLocalState() != EndpointState.CLOSED) {
                    ErrorCondition condition =
                            new ErrorCondition(Symbol.getSymbol("amqp:resource-limit-exceeded"),
                                                                "local-idle-timeout expired");
                    _connectionEndpoint.setCondition(condition);
                    _connectionEndpoint.setLocalState(EndpointState.CLOSED);

                    if (!_isOpenSent) {
                        if ((_sasl != null) && (!_sasl.isDone())) {
                            _sasl.fail();
                        }
                        Open open = new Open();
                        _isOpenSent = true;
                        writeFrame(0, open, null, null);
                    }
                    if (!_isCloseSent) {
                        Close close = new Close();
                        close.setError(condition);
                        _isCloseSent = true;
                        writeFrame(0, close, null, null);
                    }
                    close_tail();
                }
            }
            timeout = _localIdleDeadline;
        }

        if (_remoteIdleTimeout != 0 && !_isCloseSent) {
            if (_remoteIdleDeadline == 0 || _lastBytesOutput != _bytesOutput) {
                _remoteIdleDeadline = now + (_remoteIdleTimeout / 2);
                _lastBytesOutput = _bytesOutput;
            } else if (_remoteIdleDeadline <= now) {
                _remoteIdleDeadline = now + (_remoteIdleTimeout / 2);
                if (pending() == 0) {
                    writeFrame(0, null, null, null);
                    _lastBytesOutput += pending();
                }
            }
            timeout = Math.min(timeout == 0 ? _remoteIdleDeadline : timeout, _remoteIdleDeadline);
        }

        return timeout;
    }

    @Override
    public long getFramesOutput()
    {
        return _frameWriter.getFramesOutput();
    }

    @Override
    public long getFramesInput()
    {
        return _frameParser.getFramesInput();
    }

    @Override
    public void close_head()
    {
        _outputProcessor.close_head();
    }

    @Override
    public boolean isClosed() {
        int p = pending();
        int c = capacity();
        return  p == END_OF_STREAM && c == END_OF_STREAM;
    }

    @Override
    public String toString()
    {
        return "TransportImpl [_connectionEndpoint=" + _connectionEndpoint + ", " + super.toString() + "]";
    }

    private static class PartialTransfer implements Runnable
    {
        private final Transfer _transfer;

        public PartialTransfer(Transfer transfer)
        {
            _transfer = transfer;
        }

        @Override
        public void run()
        {
            _transfer.setMore(true);
        }
    }

    /**
     * Override the default frame handler. Must be called before the transport starts being used
     * (e.g. {@link #getInputBuffer()}, {@link #getOutputBuffer()}, {@link #ssl(SslDomain)} etc).
     */
    public void setFrameHandler(FrameHandler frameHandler)
    {
        _frameHandler = frameHandler;
    }

    static String INCOMING = "<-";
    static String OUTGOING = "->";

    void log(String event, TransportFrame frame)
    {
        if (isTraceFramesEnabled()) {
            StringBuilder msg = new StringBuilder();
            msg.append("[").append(System.identityHashCode(this)).append(":")
                .append(frame.getChannel()).append("]");
            msg.append(" ").append(event).append(" ").append(frame.getBody());

            Binary bin = frame.getPayload();
            if (bin != null) {
                msg.append(" (").append(bin.getLength()).append(") ");
                msg.append(StringUtils.toQuotedString(bin, TRACE_FRAME_PAYLOAD_LENGTH, true));
            }
            System.out.println(msg.toString());
        }
    }

    boolean isTraceFramesEnabled()
    {
        return (_levels & TRACE_FRM) != 0;
    }

    @Override
    void localOpen() {}

    @Override
    void localClose() {}

    public void setSelectable(Selectable selectable) {
        _selectable = selectable;
    }

    public Selectable getSelectable() {
        return _selectable;
    }

    public void setReactor(Reactor reactor) {
        _reactor = reactor;
    }

    public Reactor getReactor() {
        return _reactor;
    }

    @Override
    public void setEmitFlowEventOnSend(boolean emitFlowEventOnSend)
    {
        _emitFlowEventOnSend = emitFlowEventOnSend;
    }

    @Override
    public boolean isEmitFlowEventOnSend()
    {
        return _emitFlowEventOnSend;
    }
}
