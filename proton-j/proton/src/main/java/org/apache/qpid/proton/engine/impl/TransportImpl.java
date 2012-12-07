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

import java.nio.ByteBuffer;
import java.util.HashMap;
import java.util.Map;
import org.apache.qpid.proton.codec.CompositeWritableBuffer;
import org.apache.qpid.proton.codec.DecoderImpl;
import org.apache.qpid.proton.codec.EncoderImpl;
import org.apache.qpid.proton.codec.WritableBuffer;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.EndpointError;
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.Sasl;
import org.apache.qpid.proton.engine.Ssl;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.engine.TransportException;
import org.apache.qpid.proton.engine.impl.ssl.SslImpl;
import org.apache.qpid.proton.framing.TransportFrame;
import org.apache.qpid.proton.type.*;
import org.apache.qpid.proton.type.transport.Attach;
import org.apache.qpid.proton.type.transport.Begin;
import org.apache.qpid.proton.type.transport.Close;
import org.apache.qpid.proton.type.transport.Detach;
import org.apache.qpid.proton.type.transport.Disposition;
import org.apache.qpid.proton.type.transport.End;
import org.apache.qpid.proton.type.transport.Flow;
import org.apache.qpid.proton.type.transport.FrameBody;
import org.apache.qpid.proton.type.transport.Open;
import org.apache.qpid.proton.type.transport.Role;
import org.apache.qpid.proton.type.transport.Transfer;

public class TransportImpl extends EndpointImpl implements Transport, FrameBody.FrameBodyHandler<Integer>,FrameTransport
{
    public static final int SESSION_WINDOW = 1024;

    public static final byte[] HEADER = new byte[8];
    public static final org.apache.qpid.proton.type.messaging.Accepted ACCEPTED =
            new org.apache.qpid.proton.type.messaging.Accepted();

    static
    {
        HEADER[0] = (byte) 'A';
        HEADER[1] = (byte) 'M';
        HEADER[2] = (byte) 'Q';
        HEADER[3] = (byte) 'P';
        HEADER[4] = 0;
        HEADER[5] = 1;
        HEADER[6] = 0;
        HEADER[7] = 0;
    }

    private ConnectionImpl _connectionEndpoint;

    private boolean _isOpenSent;
    private boolean _isCloseSent;

    private boolean _headerWritten;
    private TransportSession[] _remoteSessions;
    private TransportSession[] _localSessions;

    private TransportInput _inputProcessor;
    private TransportOutput _outputProcessor;

    private Map<SessionImpl, TransportSession> _transportSessionState = new HashMap<SessionImpl, TransportSession>();
    private Map<LinkImpl, TransportLink> _transportLinkState = new HashMap<LinkImpl, TransportLink>();


    private DecoderImpl _decoder = new DecoderImpl();
    private EncoderImpl _encoder = new EncoderImpl(_decoder);

    private int _maxFrameSize = 16 * 1024;

    private final ByteBuffer _overflowBuffer = ByteBuffer.wrap(new byte[_maxFrameSize]);
    private static final byte AMQP_FRAME_TYPE = 0;
    private boolean _closeReceived;
    private Open _open;
    private SaslImpl _sasl;
    private SslImpl _ssl;
    private TransportException _inputException;
    private ProtocolTracer _protocolTracer = null;


    {
        AMQPDefinedTypes.registerAllTypes(_decoder);
        _overflowBuffer.flip();
    }

    public TransportImpl()
    {
        FrameParser frameParser = new FrameParser(this);

        _inputProcessor = frameParser;
        _outputProcessor = new TransportOutput()
                    {
                        @Override
                        public int output(byte[] bytes, int offset, int size)
                        {
                            return transportOutput(bytes, offset, size);
                        }
                    };
    }

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
            _inputProcessor.input(new byte[0],0,0);
        }
    }

    public int input(byte[] bytes, int offset, int length)
    {
        if(_inputException != null)
        {
            throw _inputException;
        }
        if(length == 0)
        {
            if(_connectionEndpoint == null || _connectionEndpoint.getRemoteState() != EndpointState.CLOSED)
            {
                throw new TransportException("Unexpected EOS: connection aborted");
            }
        }
        try
        {
            return  _inputProcessor.input(bytes, offset, length);
        }
        catch (TransportException e)
        {
            _inputException = e;
            throw e;
        }
    }

    //==================================================================================================================
    // Process model state to generate output


    public int output(byte[] bytes, final int offset, final int size)
    {
        try
        {
            return _outputProcessor.output(bytes, offset, size);
        }
        catch (RuntimeException e)
        {
            e.printStackTrace();
            throw e;
        }
    }

    private int transportOutput(byte[] bytes, int offset, int size)
    {
        int written = 0;

        if(_overflowBuffer.hasRemaining())
        {
            final int overflowWritten = Math.min(size, _overflowBuffer.remaining());
            _overflowBuffer.get(bytes, offset, overflowWritten);
            written+=overflowWritten;
        }
        if(!_overflowBuffer.hasRemaining())
        {
            _overflowBuffer.clear();

            CompositeWritableBuffer outputBuffer =
                    new CompositeWritableBuffer(
                       new WritableBuffer.ByteBufferWrapper(ByteBuffer.wrap(bytes, offset + written, size - written)),
                       new WritableBuffer.ByteBufferWrapper(_overflowBuffer));

            if( outputBuffer.remaining() >= _maxFrameSize ) { written += processHeader(outputBuffer);
            if( outputBuffer.remaining() >= _maxFrameSize ) { written += processOpen(outputBuffer);
            if( outputBuffer.remaining() >= _maxFrameSize ) { written += processBegin(outputBuffer);
            if( outputBuffer.remaining() >= _maxFrameSize ) { written += processAttach(outputBuffer);
            if( outputBuffer.remaining() >= _maxFrameSize ) { written += processReceiverDisposition(outputBuffer);
            if( outputBuffer.remaining() >= _maxFrameSize ) { written += processReceiverFlow(outputBuffer);
            if( outputBuffer.remaining() >= _maxFrameSize ) { written += processMessageData(outputBuffer);
            if( outputBuffer.remaining() >= _maxFrameSize ) { written += processSenderDisposition(outputBuffer);
            if( outputBuffer.remaining() >= _maxFrameSize ) { written += processSenderFlow(outputBuffer);
            if( outputBuffer.remaining() >= _maxFrameSize ) { written += processDetach(outputBuffer);
            if( outputBuffer.remaining() >= _maxFrameSize ) { written += processEnd(outputBuffer);
            if( outputBuffer.remaining() >= _maxFrameSize ) { written += processClose(outputBuffer);
            }}}}}}}}}}}}

            _overflowBuffer.flip();
            written -= _overflowBuffer.remaining();
        }

        return written;
    }

    public Sasl sasl()
    {
        if(_sasl == null)
        {
            _sasl = new SaslImpl();
            TransportWrapper transportWrapper = _sasl.wrap(_inputProcessor, _outputProcessor);
            _inputProcessor = transportWrapper;
            _outputProcessor = transportWrapper;
        }
        return _sasl;

    }

    @Override
    public Ssl ssl()
    {
        if (_ssl == null)
        {
            _ssl = new SslImpl();
            TransportWrapper transportWrapper = _ssl.wrap(_inputProcessor, _outputProcessor);
            _inputProcessor = transportWrapper;
            _outputProcessor = transportWrapper;
        }
        return _ssl;
    }

    private void clearTransportWorkList()
    {
        DeliveryImpl delivery = _connectionEndpoint.getTransportWorkHead();
        while(delivery != null)
        {
            DeliveryImpl transportWorkNext = delivery.getTransportWorkNext();
            delivery.clearTransportWork();
            delivery = transportWorkNext;
        }
    }

    private int processDetach(WritableBuffer buffer)
    {
        int written = 0;
        if(_connectionEndpoint != null)
        {
            EndpointImpl endpoint = _connectionEndpoint.getTransportHead();
            while(endpoint != null && buffer.remaining() >= _maxFrameSize)
            {

                if(endpoint instanceof LinkImpl)
                {
                    LinkImpl link = (LinkImpl) endpoint;
                    TransportLink transportLink = getTransportState(link);
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

                            EndpointError localError = link.getLocalError();
                            if( localError !=null ) {
                                org.apache.qpid.proton.type.transport.Error error = new org.apache.qpid.proton.type.transport.Error();
                                error.setCondition(Symbol.getSymbol(localError.getName()));
                                error.setDescription(localError.getDescription());
                                detach.setError(error);
                            }


                            int frameBytes = writeFrame(buffer, transportSession.getLocalChannel(), detach, null, null);
                            written += frameBytes;
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
        return written;
    }

    private int processSenderFlow(WritableBuffer buffer)
    {
        int written = 0;
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
                        int frameBytes = writeFrame(buffer, transportSession.getLocalChannel(), flow, null, null);
                        written += frameBytes;
                        endpoint.clearModified();
                    }

                }

                endpoint = endpoint.transportNext();
            }
        }
        return written;  //TODO - Implement
    }

    private int processSenderDisposition(WritableBuffer buffer)
    {
        int written = 0;
        if(_connectionEndpoint != null)
        {
            DeliveryImpl delivery = _connectionEndpoint.getTransportWorkHead();
            while(delivery != null && buffer.remaining() >= _maxFrameSize )
            {
                if((delivery.getLink() instanceof SenderImpl) && delivery.isLocalStateChange() && delivery.getTransportDelivery() != null)
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
                    int frameBytes = writeFrame(buffer, delivery.getLink().getSession()
                                                                      .getTransportSession().getLocalChannel(),
                                       disposition, null, null);
                    written += frameBytes;
                    delivery = delivery.clearTransportWork();
                }
                else
                {
                    delivery = delivery.getTransportWorkNext();
                }
            }
        }
        return written;

    }

    private int processMessageData(WritableBuffer buffer)
    {
        int written = 0;
        if(_connectionEndpoint != null)
        {
            DeliveryImpl delivery = _connectionEndpoint.getTransportWorkHead();

            while(delivery != null && buffer.remaining() >= _maxFrameSize)
            {
                if((delivery.getLink() instanceof SenderImpl) && !(delivery.isDone() && delivery.getDataLength() == 0)
                   && delivery.getLink().getSession().getTransportSession().hasOutgoingCredit())
                {
                    SenderImpl sender = (SenderImpl) delivery.getLink();

                    sender.decrementQueued();


                    TransportLink transportLink = sender.getTransportLink();

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

                    int frameBytes = writeFrame(buffer,
                                                sender.getSession().getTransportSession().getLocalChannel(),
                                                transfer, payload, new PartialTransfer(transfer));
                    sender.getSession().getTransportSession().incrementOutgoingId();

                    written += frameBytes;

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
        return written;
    }

    private int processReceiverDisposition(WritableBuffer buffer)
    {
        int written = 0;
        if(_connectionEndpoint != null)
        {
            DeliveryImpl delivery = _connectionEndpoint.getTransportWorkHead();
            while(delivery != null && buffer.remaining() >= _maxFrameSize)
            {
                boolean remove = false;
                if((delivery.getLink() instanceof ReceiverImpl) && delivery.isLocalStateChange())
                {
                    remove = true;
                    TransportDelivery transportDelivery = delivery.getTransportDelivery();
                    Disposition disposition = new Disposition();
                    disposition.setFirst(transportDelivery.getDeliveryId());
                    disposition.setLast(transportDelivery.getDeliveryId());
                    disposition.setRole(Role.RECEIVER);
                    disposition.setSettled(delivery.isSettled());

                    disposition.setState(delivery.getLocalState());
                    int frameBytes = writeFrame(buffer, delivery.getLink().getSession()
                                                                      .getTransportSession().getLocalChannel(),
                                       disposition, null, null);
                    written += frameBytes;
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
        return written;
    }

    private int processReceiverFlow(WritableBuffer buffer)
    {
        int written = 0;
        if(_connectionEndpoint != null)
        {
            EndpointImpl endpoint = _connectionEndpoint.getTransportHead();
            while(endpoint != null && buffer.remaining() >= _maxFrameSize)
            {

                if(endpoint instanceof ReceiverImpl)
                {

                    ReceiverImpl receiver = (ReceiverImpl) endpoint;
                    TransportLink transportLink = getTransportState(receiver);
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
                            int frameBytes = writeFrame(buffer, transportSession.getLocalChannel(), flow, null, null);
                            written += frameBytes;
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
                            int frameBytes = writeFrame(buffer, transportSession.getLocalChannel(), flow, null, null);
                            written += frameBytes;
                        }
                    }
                }
                endpoint = endpoint.transportNext();
            }
        }
        return written;
    }

    private int processAttach(WritableBuffer buffer)
    {
        int written = 0;
        if(_connectionEndpoint != null)
        {
            EndpointImpl endpoint = _connectionEndpoint.getTransportHead();

            while(endpoint != null && buffer.remaining() >= _maxFrameSize)
            {
                if(endpoint instanceof LinkImpl)
                {

                    LinkImpl link = (LinkImpl) endpoint;
                    TransportLink transportLink = getTransportState(link);
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

                            int frameBytes = writeFrame(buffer, transportSession.getLocalChannel(), attach, null, null);
                            written += frameBytes;
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
        return written;
    }

    private void clearInterestList()
    {
        EndpointImpl endpoint = _connectionEndpoint.getTransportHead();
        while(endpoint != null)
        {
            endpoint.clearModified();
            endpoint = endpoint.transportNext();
        }
    }

    private int processHeader(WritableBuffer buffer)
    {
        if(!_headerWritten)
        {
            buffer.put(HEADER, 0, HEADER.length);
            _headerWritten = true;
            return HEADER.length;
        }
        else
        {
            return 0;
        }
    }

    private int processOpen(WritableBuffer buffer)
    {
        if(_connectionEndpoint != null && _connectionEndpoint.getLocalState() != EndpointState.UNINITIALIZED && !_isOpenSent)
        {
            Open open = new Open();
            open.setContainerId(_connectionEndpoint.getLocalContainerId());
            open.setHostname(_connectionEndpoint.getHostname());
            // TODO - populate;

            _isOpenSent = true;

            return  writeFrame(buffer, 0, open, null, null);

        }
        return 0;
    }

    private int processBegin(WritableBuffer buffer)
    {
        int written = 0;

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

                        written += writeFrame(buffer, channelId, begin, null, null);
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
        return written;
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

    private TransportLink getTransportState(LinkImpl link)
    {
        TransportLink transportLink = _transportLinkState.get(link);
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

    private int processEnd(WritableBuffer buffer)
    {
        int written = 0;
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
                    int frameBytes = writeFrame(buffer, channel, end, null, null);
                    written += frameBytes;
                    endpoint.clearModified();
                }

                endpoint = endpoint.transportNext();

            }
        }
        return written;
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

    private int processClose(WritableBuffer buffer)
    {
        if(_connectionEndpoint != null && _connectionEndpoint.getLocalState() == EndpointState.CLOSED && !_isCloseSent)
        {
            if(!hasSendableMessages(null))
            {
                Close close = new Close();

                // TODO - populate;

                _isCloseSent = true;

                return  writeFrame(buffer, 0, close, null, null);
            }
        }
        return 0;

    }

    private int writeFrame(WritableBuffer buffer,
                           int channel,
                           DescribedType frameBody,
                           ByteBuffer payload,
                           Runnable onPayloadTooLarge)
    {
        int oldPosition = buffer.position();
        buffer.position(oldPosition+8);
        _encoder.setByteBuffer(buffer);

        _encoder.writeDescribedType(frameBody);

        if(payload != null && (payload.remaining() + buffer.position() - oldPosition) > _maxFrameSize)
        {
            if(onPayloadTooLarge != null)
            {
                onPayloadTooLarge.run();
            }
            buffer.position(oldPosition+8);
            _encoder.writeDescribedType(frameBody);
        }

        if( _protocolTracer!=null )
        {
            ByteBuffer originalPayload = null;
            if( payload!=null )
            {
                originalPayload = payload.duplicate();
            }
            _protocolTracer.sentFrame(new TransportFrame(channel, (FrameBody) frameBody, Binary.create(originalPayload)));
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

        return frameSize;
    }

    //==================================================================================================================

    @Override
    protected ConnectionImpl getConnectionImpl()
    {
        return _connectionEndpoint;
    }

    public void free()
    {
        super.free();
    }

    //==================================================================================================================
    // handle incoming amqp data


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

        if(open.getMaxFrameSize().longValue() > 0 && open.getMaxFrameSize().longValue() < (long) _maxFrameSize)
        {
            _maxFrameSize = (int) open.getMaxFrameSize().longValue();
        }
    }

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
            TransportLink transportLink = transportSession.getLinkFromRemoteHandle(attach.getHandle());
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

    public void handleDetach(Detach detach, Binary payload, Integer channel)
    {
        TransportSession transportSession = _remoteSessions[channel];
        if(transportSession == null)
        {
            // TODO - fail due to attach on non-begun session
        }
        else
        {
            TransportLink transportLink = transportSession.getLinkFromRemoteHandle(detach.getHandle());

            if(transportLink != null)
            {
                LinkImpl link = transportLink.getLink();
                transportLink.receivedDetach();
                transportSession.freeRemoteHandle(transportLink.getRemoteHandle());
                link.setRemoteState(EndpointState.CLOSED);

            }
            else
            {
                // TODO - fail - attempt attach on a handle which is in use
            }
        }
    }

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
            transportSession.getSession().setRemoteState(EndpointState.CLOSED);

        }
    }

    public void handleClose(Close close, Binary payload, Integer channel)
    {
        _closeReceived = true;
        setRemoteState(EndpointState.CLOSED);
        if(_connectionEndpoint != null)
        {
            _connectionEndpoint.setRemoteState(EndpointState.CLOSED);
        }

    }

    public boolean input(TransportFrame frame)
    {
        if( _protocolTracer!=null )
        {
            _protocolTracer.receivedFrame(frame);
        }
        if(_connectionEndpoint != null || getRemoteState() == EndpointState.UNINITIALIZED)
        {
            frame.getBody().invoke(this,frame.getPayload(), frame.getChannel());
            return true;
        }
        else
        {
            return false;
        }
    }

    private static class PartialTransfer implements Runnable
    {
        private final Transfer _transfer;

        public PartialTransfer(Transfer transfer)
        {
            _transfer = transfer;
        }

        public void run()
        {
            _transfer.setMore(true);
        }
    }

    public ProtocolTracer getProtocolTracer()
    {
        return _protocolTracer;
    }

    public void setProtocolTracer(ProtocolTracer protocolTracer)
    {
        this._protocolTracer = protocolTracer;
    }

}
