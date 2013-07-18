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

import static org.apache.qpid.proton.engine.impl.ByteBufferUtils.newReadableBuffer;
import static org.apache.qpid.proton.engine.impl.ByteBufferUtils.pour;
import static org.apache.qpid.proton.engine.impl.ByteBufferUtils.pourArrayToBuffer;
import static org.apache.qpid.proton.engine.impl.ByteBufferUtils.pourBufferToArray;

import java.nio.ByteBuffer;
import java.util.HashMap;
import java.util.Map;

import org.apache.qpid.proton.amqp.Binary;
import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.amqp.UnsignedShort;
import org.apache.qpid.proton.amqp.transport.Attach;
import org.apache.qpid.proton.amqp.transport.Begin;
import org.apache.qpid.proton.amqp.transport.Close;
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
import org.apache.qpid.proton.codec.CompositeWritableBuffer;
import org.apache.qpid.proton.codec.DecoderImpl;
import org.apache.qpid.proton.codec.EncoderImpl;
import org.apache.qpid.proton.codec.WritableBuffer;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.EngineFactory;
import org.apache.qpid.proton.engine.EngineLogger;
import org.apache.qpid.proton.engine.ProtonJTransport;
import org.apache.qpid.proton.engine.Sasl;
import org.apache.qpid.proton.engine.Ssl;
import org.apache.qpid.proton.engine.SslDomain;
import org.apache.qpid.proton.engine.SslPeerDetails;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.engine.TransportException;
import org.apache.qpid.proton.engine.TransportResult;
import org.apache.qpid.proton.engine.TransportResultFactory;
import org.apache.qpid.proton.engine.impl.ssl.ProtonSslEngineProvider;
import org.apache.qpid.proton.engine.impl.ssl.SslImpl;
import org.apache.qpid.proton.framing.TransportFrame;
import org.apache.qpid.proton.logging.ProtonLogger;

public class TransportImpl extends EndpointImpl
    implements ProtonJTransport, FrameBody.FrameBodyHandler<Integer>,
        FrameHandler, TransportOutputWriter
{
    private static final byte AMQP_FRAME_TYPE = 0;

    private FrameParser _frameParser;

    private ConnectionImpl _connectionEndpoint;

    private boolean _isOpenSent;
    private boolean _isCloseSent;

    private boolean _headerWritten;
    private TransportSession[] _remoteSessions;
    private TransportSession[] _localSessions;

    private TransportInput _inputProcessor;
    private TransportOutput _outputProcessor;

    private Map<SessionImpl, TransportSession> _transportSessionState = new HashMap<SessionImpl, TransportSession>();
    private Map<LinkImpl, TransportLink<?>> _transportLinkState = new HashMap<LinkImpl, TransportLink<?>>();


    private DecoderImpl _decoder = new DecoderImpl();
    private EncoderImpl _encoder = new EncoderImpl(_decoder);

    private int _maxFrameSize = DEFAULT_MAX_FRAME_SIZE;

    private ByteBuffer _outputOverflowBuffer;

    private boolean _closeReceived;
    private Open _open;
    private SaslImpl _sasl;
    private SslImpl _ssl;
    private ProtocolTracer _protocolTracer = null;

    private ByteBuffer _lastInputBuffer;

    private TransportResult _lastTransportResult = TransportResultFactory.ok();

    private boolean _init;

    private EngineLogger _engineLogger;

    private FrameHandler _frameHandler = this;

    /**
     * @deprecated This constructor's visibility will be reduced to the default scope in a future release.
     * Client code outside this module should use a {@link EngineFactory} instead
     */
    @Deprecated public TransportImpl(EngineLogger engineLogger)
    {
        this(engineLogger, DEFAULT_MAX_FRAME_SIZE);
    }


    /**
     * Creates a transport with the given maximum frame size.
     * Note that the maximumFrameSize also determines the size of the output buffer.
     */
    TransportImpl(EngineLogger engineLogger, int maxFrameSize)
    {
        AMQPDefinedTypes.registerAllTypes(_decoder, _encoder);

        _engineLogger = engineLogger;
        _maxFrameSize = maxFrameSize;

    }

    /**
     * This constructor is intended to only be used by tests because it uses a hard-coded logger implementation
     */
    TransportImpl()
    {
        this(new ProtonLogger());
    }

    /**
     * This constructor is intended to only be used by tests because it uses a hard-coded logger implementation
     */
    TransportImpl(int maxFrameSize)
    {
        this(new ProtonLogger(), maxFrameSize);
    }

    private void init()
    {
        if(!_init)
        {
            _init = true;
            _frameParser = new FrameParser(_frameHandler , _decoder, _maxFrameSize);
            _inputProcessor = _frameParser;
            _outputProcessor = new TransportOutputAdaptor(this, _maxFrameSize);
            _outputOverflowBuffer = newReadableBuffer(_maxFrameSize);
        }
    }

    @Override
    public int getMaxFrameSize()
    {
        return _maxFrameSize;
    }

    /**
     * TODO propagate the new value to {@link #_outputProcessor} etc
     */
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
    public void bind(Connection conn)
    {
        // TODO - check if already bound
        ((ConnectionImpl) conn).setBound(true);
        _connectionEndpoint = (ConnectionImpl) conn;

        _localSessions = new TransportSession[_connectionEndpoint.getMaxChannels()+1];
        _remoteSessions = new TransportSession[_connectionEndpoint.getMaxChannels()+1];


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
     * @see Transport#input(byte[], int, int)
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
    public void writeInto(ByteBuffer outputBuffer)
    {
        if(_outputOverflowBuffer.hasRemaining())
        {
            pour(_outputOverflowBuffer, outputBuffer);
        }
        if(!_outputOverflowBuffer.hasRemaining())
        {
            _outputOverflowBuffer.clear();

            CompositeWritableBuffer compositeOutputBuffer =
                    new CompositeWritableBuffer(
                       new WritableBuffer.ByteBufferWrapper(outputBuffer),
                       new WritableBuffer.ByteBufferWrapper(_outputOverflowBuffer));

            if( compositeOutputBuffer.remaining() >= _maxFrameSize ) { processHeader(compositeOutputBuffer);
            if( compositeOutputBuffer.remaining() >= _maxFrameSize ) { processOpen(compositeOutputBuffer);
            if( compositeOutputBuffer.remaining() >= _maxFrameSize ) { processBegin(compositeOutputBuffer);
            if( compositeOutputBuffer.remaining() >= _maxFrameSize ) { processAttach(compositeOutputBuffer);
            if( compositeOutputBuffer.remaining() >= _maxFrameSize ) { processReceiverDisposition(compositeOutputBuffer);
            if( compositeOutputBuffer.remaining() >= _maxFrameSize ) { processReceiverFlow(compositeOutputBuffer);
            if( compositeOutputBuffer.remaining() >= _maxFrameSize ) { processMessageData(compositeOutputBuffer);
            if( compositeOutputBuffer.remaining() >= _maxFrameSize ) { processSenderDisposition(compositeOutputBuffer);
            if( compositeOutputBuffer.remaining() >= _maxFrameSize ) { processSenderFlow(compositeOutputBuffer);
            if( compositeOutputBuffer.remaining() >= _maxFrameSize ) { processDetach(compositeOutputBuffer);
            if( compositeOutputBuffer.remaining() >= _maxFrameSize ) { processEnd(compositeOutputBuffer);
            if( compositeOutputBuffer.remaining() >= _maxFrameSize ) { processClose(compositeOutputBuffer);
            }}}}}}}}}}}}

            _outputOverflowBuffer.flip();
        }
    }

    @Override
    public Sasl sasl()
    {
        if(_sasl == null)
        {
            init();
            _sasl = new SaslImpl(_maxFrameSize);
            TransportWrapper transportWrapper = _sasl.wrap(_inputProcessor, _outputProcessor);
            _inputProcessor = transportWrapper;
            _outputProcessor = transportWrapper;
        }
        return _sasl;

    }

    /**
     * {@inheritDoc}
     *
     * <p>Note that sslDomain must implement {@link ProtonSslEngineProvider}. This is not possible
     * enforce at the API level because {@link ProtonSslEngineProvider} is not part of the
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

    private void processDetach(WritableBuffer buffer)
    {
        if(_connectionEndpoint != null)
        {
            EndpointImpl endpoint = _connectionEndpoint.getTransportHead();
            while(endpoint != null && buffer.remaining() >= _maxFrameSize)
            {

                if(endpoint instanceof LinkImpl)
                {
                    LinkImpl link = (LinkImpl) endpoint;
                    TransportLink<?> transportLink = getTransportState(link);
                    SessionImpl session = link.getSession();
                    TransportSession transportSession = getTransportState(session);

                    if(link.getLocalState() == EndpointState.CLOSED
                       && transportLink.isLocalHandleSet())
                    {
                        if(!(link instanceof SenderImpl)
                           || link.getQueued() == 0
                           || transportLink.detachReceived()
                           || transportSession.endReceived()
                           || _closeReceived)
                        {
                            UnsignedInteger localHandle = transportLink.getLocalHandle();
                            transportLink.clearLocalHandle();
                            transportSession.freeLocalHandle(localHandle);


                            Detach detach = new Detach();
                            detach.setHandle(localHandle);
                            // TODO - need an API for detaching rather than closing the link
                            detach.setClosed(true);

                            ErrorCondition localError = link.getCondition();
                            if( localError.getCondition() !=null )
                            {
                                detach.setError(localError);
                            }


                            writeFrame(buffer, transportSession.getLocalChannel(), detach, null, null);
                            endpoint.clearModified();

                            // TODO - temporary hack for PROTON-154, this line should be removed and replaced
                            //        with proper handling for closed links
                            link.free();
                        }
                    }

                }
                endpoint = endpoint.transportNext();
            }
        }
    }

    private void processSenderFlow(WritableBuffer buffer)
    {
        if(_connectionEndpoint != null)
        {
            EndpointImpl endpoint = _connectionEndpoint.getTransportHead();
            while(endpoint != null && buffer.remaining() >= _maxFrameSize)
            {

                if(endpoint instanceof SenderImpl)
                {
                    SenderImpl sender = (SenderImpl) endpoint;
                    if(sender.getDrain() && sender.clearDrained())
                    {
                        TransportSender transportLink = sender.getTransportLink();
                        TransportSession transportSession = sender.getSession().getTransportSession();
                        UnsignedInteger credits = transportLink.getLinkCredit();
                        transportLink.setLinkCredit(UnsignedInteger.valueOf(0));
                        transportLink.setDeliveryCount(transportLink.getDeliveryCount().add(credits));
                        transportLink.setLinkCredit(UnsignedInteger.ZERO);

                        Flow flow = new Flow();
                        flow.setHandle(transportLink.getLocalHandle());
                        flow.setNextIncomingId(transportSession.getNextIncomingId());
                        flow.setIncomingWindow(transportSession.getIncomingWindowSize());
                        flow.setOutgoingWindow(transportSession.getOutgoingWindowSize());
                        flow.setDeliveryCount(transportLink.getDeliveryCount());
                        flow.setLinkCredit(transportLink.getLinkCredit());
                        flow.setDrain(sender.getDrain());
                        flow.setNextOutgoingId(transportSession.getNextOutgoingId());
                        writeFrame(buffer, transportSession.getLocalChannel(), flow, null, null);
                        endpoint.clearModified();
                    }

                }

                endpoint = endpoint.transportNext();
            }
        }
    }

    private void processSenderDisposition(WritableBuffer buffer)
    {
        if(_connectionEndpoint != null)
        {
            DeliveryImpl delivery = _connectionEndpoint.getTransportWorkHead();
            while(delivery != null && buffer.remaining() >= _maxFrameSize )
            {
                if((delivery.getLink() instanceof SenderImpl) && delivery.getTransportDelivery() != null)
                {
                    TransportDelivery transportDelivery = delivery.getTransportDelivery();
                    Disposition disposition = new Disposition();
                    disposition.setFirst(transportDelivery.getDeliveryId());
                    disposition.setLast(transportDelivery.getDeliveryId());
                    disposition.setRole(Role.SENDER);
                    disposition.setSettled(delivery.isSettled());
                    if(delivery.isSettled())
                    {
                        transportDelivery.settled();
                    }
                    disposition.setState(delivery.getLocalState());

                    writeFrame(buffer,
                               delivery.getLink().getSession().getTransportSession().getLocalChannel(),
                               disposition, null, null);

                    delivery = delivery.clearTransportWork();
                }
                else
                {
                    delivery = delivery.getTransportWorkNext();
                }
            }
        }
    }

    private void processMessageData(WritableBuffer buffer)
    {
        if(_connectionEndpoint != null)
        {
            DeliveryImpl delivery = _connectionEndpoint.getTransportWorkHead();

            while(delivery != null && buffer.remaining() >= _maxFrameSize)
            {
                if((delivery.getLink() instanceof SenderImpl) && !(delivery.isDone() && delivery.getDataLength() == 0)
                   && delivery.getLink().getSession().getTransportSession().hasOutgoingCredit() &&
                   delivery.getLink().getTransportLink().hasCredit())
                {
                    SenderImpl sender = (SenderImpl) delivery.getLink();

                    sender.decrementQueued();


                    TransportLink<?> transportLink = sender.getTransportLink();

                    UnsignedInteger deliveryId = transportLink.getDeliveryCount();
                    TransportDelivery transportDelivery = new TransportDelivery(deliveryId, delivery, transportLink);
                    delivery.setTransportDelivery(transportDelivery);

                    final Transfer transfer = new Transfer();
                    transfer.setDeliveryId(deliveryId);
                    transfer.setDeliveryTag(new Binary(delivery.getTag()));
                    transfer.setHandle(transportLink.getLocalHandle());

                    if(delivery.isSettled())
                    {
                        transfer.setSettled(Boolean.TRUE);
                    }
                    else
                    {
                        sender.getSession().getTransportSession().addUnsettledOutgoing(deliveryId, delivery);
                    }

                    if(delivery.getLink().current() == delivery)
                    {
                        transfer.setMore(true);
                    }

                    transfer.setMessageFormat(UnsignedInteger.ZERO);

                    // TODO - large frames
                    ByteBuffer payload = delivery.getData() ==  null ? null : ByteBuffer.wrap(delivery.getData(), delivery.getDataOffset(), delivery.getDataLength());

                    writeFrame(buffer,
                               sender.getSession().getTransportSession().getLocalChannel(),
                               transfer, payload, new PartialTransfer(transfer));
                    sender.getSession().getTransportSession().incrementOutgoingId();

                    if(payload == null || !payload.hasRemaining())
                    {
                        delivery.setData(null);
                        delivery.setDataLength(0);
                        delivery.setDone();

                        if(delivery.getLink().current() != delivery)
                        {
                            transportLink.setDeliveryCount(transportLink.getDeliveryCount().add(UnsignedInteger.ONE));
                            transportLink.setLinkCredit(transportLink.getLinkCredit().subtract(UnsignedInteger.ONE));
                        }

                        delivery = delivery.clearTransportWork();

                    }
                    else
                    {
                        delivery.setDataOffset(delivery.getDataOffset()+delivery.getDataLength()-payload.remaining());
                        delivery.setDataLength(payload.remaining());
                    }
                }
                else
                {
                    delivery = delivery.getTransportWorkNext();
                }
            }
        }
    }

    private void processReceiverDisposition(WritableBuffer buffer)
    {
        if(_connectionEndpoint != null)
        {
            DeliveryImpl delivery = _connectionEndpoint.getTransportWorkHead();
            while(delivery != null && buffer.remaining() >= _maxFrameSize)
            {
                if((delivery.getLink() instanceof ReceiverImpl))
                {
                    TransportDelivery transportDelivery = delivery.getTransportDelivery();
                    Disposition disposition = new Disposition();
                    disposition.setFirst(transportDelivery.getDeliveryId());
                    disposition.setLast(transportDelivery.getDeliveryId());
                    disposition.setRole(Role.RECEIVER);
                    disposition.setSettled(delivery.isSettled());

                    disposition.setState(delivery.getLocalState());
                    writeFrame(buffer,
                               delivery.getLink().getSession().getTransportSession().getLocalChannel(),
                               disposition, null, null);
                    if(delivery.isSettled())
                    {
                        transportDelivery.settled();
                    }
                    delivery = delivery.clearTransportWork();
                }
                else
                {
                    delivery = delivery.getTransportWorkNext();
                }
            }
        }
    }

    private void processReceiverFlow(WritableBuffer buffer)
    {
        if(_connectionEndpoint != null)
        {
            EndpointImpl endpoint = _connectionEndpoint.getTransportHead();
            while(endpoint != null && buffer.remaining() >= _maxFrameSize)
            {
                if(endpoint instanceof ReceiverImpl)
                {
                    ReceiverImpl receiver = (ReceiverImpl) endpoint;
                    TransportLink<?> transportLink = getTransportState(receiver);
                    TransportSession transportSession = getTransportState(receiver.getSession());

                    if(receiver.getLocalState() == EndpointState.ACTIVE)
                    {
                        int credits = receiver.clearUnsentCredits();
                        transportSession.getSession().clearIncomingWindowResize();
                        if(credits != 0 || receiver.getDrain())
                        {
                            transportLink.addCredit(credits);
                            Flow flow = new Flow();
                            flow.setHandle(transportLink.getLocalHandle());
                            flow.setNextIncomingId(transportSession.getNextIncomingId());
                            flow.setIncomingWindow(transportSession.getIncomingWindowSize());
                            flow.setOutgoingWindow(transportSession.getOutgoingWindowSize());
                            flow.setDeliveryCount(transportLink.getDeliveryCount());
                            flow.setLinkCredit(transportLink.getLinkCredit());
                            flow.setDrain(receiver.getDrain());
                            flow.setNextOutgoingId(transportSession.getNextOutgoingId());
                            writeFrame(buffer, transportSession.getLocalChannel(), flow, null, null);
                            if(receiver.getLocalState() == EndpointState.ACTIVE)
                            {
                                endpoint.clearModified();
                            }
                        }
                    }
                }
                endpoint = endpoint.transportNext();
            }
            endpoint = _connectionEndpoint.getTransportHead();
            while(endpoint != null && buffer.remaining() >= _maxFrameSize)
            {
                if(endpoint instanceof SessionImpl)
                {

                    SessionImpl session = (SessionImpl) endpoint;
                    TransportSession transportSession = getTransportState(session);

                    if(session.getLocalState() == EndpointState.ACTIVE)
                    {
                        boolean windowResized = session.clearIncomingWindowResize();
                        if(windowResized)
                        {
                            Flow flow = new Flow();
                            flow.setIncomingWindow(transportSession.getIncomingWindowSize());
                            flow.setOutgoingWindow(transportSession.getOutgoingWindowSize());
                            flow.setNextOutgoingId(transportSession.getNextOutgoingId());
                            flow.setNextIncomingId(transportSession.getNextIncomingId());
                            writeFrame(buffer, transportSession.getLocalChannel(), flow, null, null);
                        }
                    }
                }
                endpoint = endpoint.transportNext();
            }
        }
    }

    private void processAttach(WritableBuffer buffer)
    {
        if(_connectionEndpoint != null)
        {
            EndpointImpl endpoint = _connectionEndpoint.getTransportHead();

            while(endpoint != null && buffer.remaining() >= _maxFrameSize)
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

                            attach.setRole(endpoint instanceof ReceiverImpl ? Role.RECEIVER : Role.SENDER);

                            if(link instanceof SenderImpl)
                            {
                                attach.setInitialDeliveryCount(UnsignedInteger.ZERO);
                            }

                            writeFrame(buffer, transportSession.getLocalChannel(), attach, null, null);
                            transportLink.sentAttach();
                            if(link.getLocalState() == EndpointState.ACTIVE && (link instanceof SenderImpl || !link.hasCredit()))
                            {
                                endpoint.clearModified();
                            }
                        }
                    }
                }
                endpoint = endpoint.transportNext();
            }
        }
    }

    private void processHeader(WritableBuffer buffer)
    {
        if(!_headerWritten)
        {
            buffer.put(AmqpHeader.HEADER, 0, AmqpHeader.HEADER.length);
            _headerWritten = true;
        }
    }

    private void processOpen(WritableBuffer buffer)
    {
        if(_connectionEndpoint != null && _connectionEndpoint.getLocalState() != EndpointState.UNINITIALIZED && !_isOpenSent)
        {
            Open open = new Open();
            open.setContainerId(_connectionEndpoint.getLocalContainerId());
            open.setHostname(_connectionEndpoint.getHostname());
            open.setDesiredCapabilities(_connectionEndpoint.getDesiredCapabilities());
            open.setOfferedCapabilities(_connectionEndpoint.getOfferedCapabilities());
            open.setProperties(_connectionEndpoint.getProperties());
            // TODO - populate;

            _isOpenSent = true;

            writeFrame(buffer, 0, open, null, null);

        }
    }

    private void processBegin(WritableBuffer buffer)
    {
        if(_connectionEndpoint != null)
        {
            EndpointImpl endpoint = _connectionEndpoint.getTransportHead();
            while(endpoint != null && buffer.remaining() >= _maxFrameSize)
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
                        begin.setHandleMax(transportSession.getHandleMax());
                        begin.setIncomingWindow(transportSession.getIncomingWindowSize());
                        begin.setOutgoingWindow(transportSession.getOutgoingWindowSize());
                        begin.setNextOutgoingId(transportSession.getNextOutgoingId());

                        writeFrame(buffer, channelId, begin, null, null);
                        transportSession.sentBegin();
                        if(session.getLocalState() == EndpointState.ACTIVE)
                        {
                            endpoint.clearModified();
                        }
                    }
                }
                endpoint = endpoint.transportNext();
            }
        }
    }

    private TransportSession getTransportState(SessionImpl session)
    {
        TransportSession transportSession = _transportSessionState.get(session);
        if(transportSession == null)
        {
            transportSession = new TransportSession(session);
            session.setTransportSession(transportSession);
            _transportSessionState.put(session, transportSession);
        }
        return transportSession;
    }

    private TransportLink<?> getTransportState(LinkImpl link)
    {
        TransportLink<?> transportLink = _transportLinkState.get(link);
        if(transportLink == null)
        {
            transportLink = TransportLink.createTransportLink(link);
            _transportLinkState.put(link, transportLink);
        }
        return transportLink;
    }

    private int allocateLocalChannel(TransportSession transportSession)
    {
        for( int i=0; i < _localSessions.length; i++)
        {
            if( _localSessions[i] == null )
            {
                _localSessions[i] = transportSession;
                transportSession.setLocalChannel(i);
                return i;
            }
        }
        return -1;
    }

    private int freeLocalChannel(TransportSession transportSession)
    {
        final int channel = transportSession.getLocalChannel();
        _localSessions[channel] = null;
        transportSession.freeLocalChannel();
        return channel;
    }

    private void processEnd(WritableBuffer buffer)
    {
        if(_connectionEndpoint != null)
        {
            EndpointImpl endpoint = _connectionEndpoint.getTransportHead();
            while(endpoint != null && buffer.remaining() >= _maxFrameSize)
            {
                SessionImpl session;
                TransportSession transportSession;

                if((endpoint instanceof SessionImpl)
                   && (session = (SessionImpl)endpoint).getLocalState() == EndpointState.CLOSED
                   && (transportSession = session.getTransportSession()).isLocalChannelSet()
                   && !hasSendableMessages(session))
                {
                    int channel = freeLocalChannel(transportSession);
                    End end = new End();
                    ErrorCondition localError = endpoint.getCondition();
                    if( localError.getCondition() !=null )
                    {
                        end.setError(localError);
                    }

                    writeFrame(buffer, channel, end, null, null);
                    endpoint.clearModified();
                }

                endpoint = endpoint.transportNext();
            }
        }
    }

    private boolean hasSendableMessages(SessionImpl session)
    {

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

    private void processClose(WritableBuffer buffer)
    {
        if(_connectionEndpoint != null && _connectionEndpoint.getLocalState() == EndpointState.CLOSED && !_isCloseSent)
        {
            if(!hasSendableMessages(null))
            {
                Close close = new Close();

                ErrorCondition localError = _connectionEndpoint.getCondition();
                if( localError.getCondition() !=null )
                {
                    close.setError(localError);
                }

                _isCloseSent = true;

                writeFrame(buffer, 0, close, null, null);
            }
        }
    }

    private void writeFrame(WritableBuffer buffer,
                           int channel,
                           FrameBody frameBody,
                           ByteBuffer payload,
                           Runnable onPayloadTooLarge)
    {
        int oldPosition = buffer.position();
        buffer.position(oldPosition+8);
        _encoder.setByteBuffer(buffer);

        _encoder.writeObject(frameBody);

        if(payload != null && (payload.remaining() + buffer.position() - oldPosition) > _maxFrameSize)
        {
            if(onPayloadTooLarge != null)
            {
                onPayloadTooLarge.run();
            }
            buffer.position(oldPosition+8);
            _encoder.writeObject(frameBody);
        }

        ByteBuffer originalPayload = null;
        if( payload!=null )
        {
            originalPayload = payload.duplicate();
        }
        TransportFrame frame = new TransportFrame(channel, frameBody, Binary.create(originalPayload));
        log(OUTGOING, frame);

        if( _protocolTracer!=null )
        {
            _protocolTracer.sentFrame(frame);
        }

        int payloadSize = Math.min(payload == null ? 0 : payload.remaining(), _maxFrameSize - (buffer.position() - oldPosition));
        if(payloadSize > 0)
        {
            int oldLimit = payload.limit();
            payload.limit(payload.position() + payloadSize);
            buffer.put(payload);
            payload.limit(oldLimit);
        }
        int frameSize = buffer.position() - oldPosition;
        int limit = buffer.position();
        buffer.position(oldPosition);
        buffer.putInt(frameSize);
        buffer.put((byte) 2);
        buffer.put(AMQP_FRAME_TYPE);
        buffer.putShort((short) channel);
        buffer.position(limit);
    }

    //==================================================================================================================

    @Override
    protected ConnectionImpl getConnectionImpl()
    {
        return _connectionEndpoint;
    }

    @Override
    public void free()
    {
        super.free();
    }

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

        if(open.getMaxFrameSize().longValue() > 0 && open.getMaxFrameSize().longValue() < _maxFrameSize)
        {
            _maxFrameSize = (int) open.getMaxFrameSize().longValue();
        }
    }

    @Override
    public void handleBegin(Begin begin, Binary payload, Integer channel)
    {
        // TODO - check channel < max_channel
        TransportSession transportSession = _remoteSessions[channel];
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
                // TODO check null
                transportSession = _localSessions[begin.getRemoteChannel().intValue()];
                session = transportSession.getSession();

            }
            transportSession.setRemoteChannel(channel);
            session.setRemoteState(EndpointState.ACTIVE);
            transportSession.setNextIncomingId(begin.getNextOutgoingId());
            _remoteSessions[channel] = transportSession;


        }

    }

    @Override
    public void handleAttach(Attach attach, Binary payload, Integer channel)
    {
        TransportSession transportSession = _remoteSessions[channel];
        if(transportSession == null)
        {
            // TODO - fail due to attach on non-begun session
        }
        else
        {
            SessionImpl session = transportSession.getSession();
            TransportLink<?> transportLink = transportSession.getLinkFromRemoteHandle(attach.getHandle());
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

                transportLink.setName(attach.getName());
                transportLink.setRemoteHandle(attach.getHandle());
                transportSession.addLinkRemoteHandle(transportLink, attach.getHandle());

            }
        }
    }

    @Override
    public void handleFlow(Flow flow, Binary payload, Integer channel)
    {
        TransportSession transportSession = _remoteSessions[channel];
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
        TransportSession transportSession = _remoteSessions[channel];
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
        TransportSession transportSession = _remoteSessions[channel];
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
        TransportSession transportSession = _remoteSessions[channel];
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
        TransportSession transportSession = _remoteSessions[channel];
        if(transportSession == null)
        {
            // TODO - fail due to attach on non-begun session
        }
        else
        {
            _remoteSessions[channel] = null;
            transportSession.receivedEnd();
            SessionImpl session = transportSession.getSession();
            session.setRemoteState(EndpointState.CLOSED);
            ErrorCondition errorCondition = end.getError();
            if(errorCondition != null)
            {
                session.getRemoteCondition().copyFrom(errorCondition);
            }
        }
    }

    @Override
    public void handleClose(Close close, Binary payload, Integer channel)
    {
        _closeReceived = true;
        setRemoteState(EndpointState.CLOSED);
        if(_connectionEndpoint != null)
        {
            _connectionEndpoint.setRemoteState(EndpointState.CLOSED);
            if(close.getError() != null)
            {
                _connectionEndpoint.getRemoteCondition().copyFrom(close.getError());
            }
        }

    }

    @Override
    public void handleFrame(TransportFrame frame)
    {
        if (!isHandlingFrames())
        {
            throw new IllegalStateException("Transport cannot accept frame: " + frame);
        }

        log(INCOMING, frame);

        if( _protocolTracer != null )
        {
            _protocolTracer.receivedFrame(frame);
        }

        frame.getBody().invoke(this,frame.getPayload(), frame.getChannel());
    }

    @Override
    public boolean isHandlingFrames()
    {
        return _connectionEndpoint != null || getRemoteState() == EndpointState.UNINITIALIZED;
    }

    @Override
    public ProtocolTracer getProtocolTracer()
    {
        return _protocolTracer;
    }

    @Override
    public void setProtocolTracer(ProtocolTracer protocolTracer)
    {
        this._protocolTracer = protocolTracer;
    }

    @Override
    public ByteBuffer getInputBuffer()
    {
        init();
        _lastTransportResult.checkIsOk();
        _lastInputBuffer = _inputProcessor.getInputBuffer();
        return _lastInputBuffer;
    }

    @Override
    public TransportResult processInput()
    {
        if (_lastInputBuffer == null)
        {
            throw new IllegalStateException("Unexpected invocation of processInput(), it is required to invoke getInputBuffer() first");
        }

        _lastTransportResult = _inputProcessor.processInput();
        return _lastTransportResult;
    }

    @Override
    public ByteBuffer getOutputBuffer()
    {
        init();
        return _outputProcessor.getOutputBuffer();
    }

    @Override
    public void outputConsumed()
    {
        _outputProcessor.outputConsumed();
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

    @Override
    public EngineLogger getEngineLogger()
    {
        return _engineLogger;
    }

    @Override
    public void setEngineLogger(EngineLogger engineLogger)
    {
        _engineLogger = engineLogger;
    }

    /**
     * Override the default frame handler. Must be called before the transport starts being used
     * (e.g. {@link #getInputBuffer()}, {@link #getOutputBuffer()}, {@link #ssl(SslDomain)} etc).
     */
    public void setFrameHandler(FrameHandler frameHandler)
    {
        _frameHandler = frameHandler;
    }

    private static String INCOMING = "<-";
    private static String OUTGOING = "->";

    private void log(String event, TransportFrame frame)
    {
        /*StringBuilder msg = new StringBuilder();
        msg.append("[").append(System.identityHashCode(this)).append(":")
            .append(frame.getChannel()).append("]");
        msg.append(" ").append(event).append(" ").append(frame.getBody());
        if (frame.getPayload() != null) {
            msg.append(" \"").append(frame.getPayload()).append("\"");
        }
        System.out.println(msg.toString());*/
    }

}
