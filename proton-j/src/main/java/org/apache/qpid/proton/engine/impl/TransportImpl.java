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

import org.apache.qpid.proton.codec.CompositeWritableBuffer;
import org.apache.qpid.proton.codec.DecoderImpl;
import org.apache.qpid.proton.codec.EncoderImpl;
import org.apache.qpid.proton.codec.WritableBuffer;
import org.apache.qpid.proton.engine.Accepted;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.DeliveryState;
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.type.AMQPDefinedTypes;
import org.apache.qpid.proton.type.Binary;
import org.apache.qpid.proton.type.DescribedType;
import org.apache.qpid.proton.type.UnsignedInteger;
import org.apache.qpid.proton.type.UnsignedShort;
import org.apache.qpid.proton.type.messaging.Source;
import org.apache.qpid.proton.type.messaging.Target;
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

import java.nio.ByteBuffer;
import java.util.HashMap;
import java.util.Map;

public class TransportImpl extends EndpointImpl implements Transport, FrameBody.FrameBodyHandler<Integer>
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

    private final FrameParser _frameParser;

    private Map<SessionImpl, TransportSession> _transportSessionState = new HashMap<SessionImpl, TransportSession>();
    private Map<LinkImpl, TransportLink> _transportLinkState = new HashMap<LinkImpl, TransportLink>();


    private DecoderImpl _decoder = new DecoderImpl();
    private EncoderImpl _encoder = new EncoderImpl(_decoder);

    private int _maxFrameSize = 16 * 1024;

    private final ByteBuffer _overflowBuffer = ByteBuffer.wrap(new byte[_maxFrameSize]);
    private static final byte AMQP_FRAME_TYPE = 0;


    {
        AMQPDefinedTypes.registerAllTypes(_decoder);
        _overflowBuffer.flip();
    }

    public TransportImpl()
    {
        _frameParser = new FrameParser(this);
    }

    public void bind(Connection conn)
    {
        // TODO - check if already bound
        ((ConnectionImpl) conn).bind(this);
        _connectionEndpoint = (ConnectionImpl) conn;
        _localSessions = new TransportSession[_connectionEndpoint.getMaxChannels()+1];
        _remoteSessions = new TransportSession[_connectionEndpoint.getMaxChannels()+1];
    }

    public int input(byte[] bytes, int offset, int length)
    {
        return _frameParser.input(bytes, offset, length);
    }

    //==================================================================================================================
    // Process model state to generate output


    public int output(byte[] bytes, final int offset, final int size)
    {
        try
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

            written += processHeader(outputBuffer);
            written += processOpen(outputBuffer);
            written += processBegin(outputBuffer);
            written += processAttach(outputBuffer);
            written += processReceiverFlow(outputBuffer);
            written += processReceiverDisposition(outputBuffer);
            written += processReceiverFlow(outputBuffer);       // TODO
            written += processMessageData(outputBuffer);
            written += processSenderDisposition(outputBuffer);
            written += processSenderFlow(outputBuffer);
            written += processDetach(outputBuffer);
            written += processEnd(outputBuffer);
            written += processClose(outputBuffer);
            _overflowBuffer.flip();
        }

        if(_overflowBuffer.position() == 0)
        {
            clearInterestList();
            //clearTransportWorkList();
        }

        return written;
        }catch (RuntimeException e)
        {
            e.printStackTrace();
            throw e;
        }
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
        EndpointImpl endpoint = _connectionEndpoint.getTransportHead();
        int written = 0;
        while(endpoint != null && buffer.remaining() >= _maxFrameSize)
        {

            if(endpoint instanceof LinkImpl)
            {

                LinkImpl link = (LinkImpl) endpoint;
                TransportLink transportLink = getTransportState(link);
                if(link.getLocalState() == EndpointState.CLOSED
                   && transportLink.isLocalHandleSet())
                {

                    SessionImpl session = link.getSession();
                    TransportSession transportSession = getTransportState(session);
                    UnsignedInteger localHandle = transportLink.getLocalHandle();
                    transportLink.clearLocalHandle();
                    transportSession.freeLocalHandle(localHandle);


                    Detach detach = new Detach();
                    detach.setHandle(localHandle);


                    int frameBytes = writeFrame(buffer, transportSession.getLocalChannel(), detach, null);
                    written += frameBytes;

                }

            }
            endpoint = endpoint.transportNext();
        }
        return written;
    }

    private int processSenderFlow(WritableBuffer buffer)
    {
        EndpointImpl endpoint = _connectionEndpoint.getTransportHead();
        int written = 0;
        while(endpoint != null && buffer.remaining() >= _maxFrameSize)
        {

            if(endpoint instanceof SenderImpl)
            {
                SenderImpl sender = (SenderImpl) endpoint;
                if(sender.getDrain() && sender.clearDrained())
                {
                    TransportSender transportLink = sender.getTransportLink();
                    TransportSession transportSession = sender.getSession().getTransportSession();
                    int credits = sender.getCredit();
                    sender.setCredit(0);
                    transportLink.setDeliveryCount(transportLink.getDeliveryCount().add(UnsignedInteger.valueOf(credits)));
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
                    int frameBytes = writeFrame(buffer, transportSession.getLocalChannel(), flow, null);
                    written += frameBytes;
                }

            }

            endpoint = endpoint.transportNext();
        }
        return written;  //TODO - Implement
    }

    private int processSenderDisposition(WritableBuffer buffer)
    {
        DeliveryImpl delivery = _connectionEndpoint.getTransportWorkHead();
        int written = 0;
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
                DeliveryState deliveryState = delivery.getLocalState();
                if(deliveryState == Accepted.getInstance())
                {
                    disposition.setState(ACCEPTED);
                }
                else
                {
                    // TODO
                }
                int frameBytes = writeFrame(buffer, delivery.getLink().getSession()
                                                                  .getTransportSession().getLocalChannel(),
                                   disposition, null);
                written += frameBytes;
                delivery = delivery.clearTransportWork();
            }
            else
            {
                delivery = delivery.getTransportWorkNext();
            }
        }
        return written;

    }

    private int processMessageData(WritableBuffer buffer)
    {
        DeliveryImpl delivery = _connectionEndpoint.getTransportWorkHead();
        int written = 0;
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
                sender.getSession().getTransportSession().addUnsettledOutgoing(deliveryId, delivery);

                Transfer transfer = new Transfer();
                transfer.setDeliveryId(deliveryId);
                transfer.setDeliveryTag(new Binary(delivery.getTag()));
                transfer.setHandle(transportLink.getLocalHandle());
                if(delivery.isSettled())
                {
                    transfer.setSettled(Boolean.TRUE);
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
                                            transfer, payload);
                sender.getSession().getTransportSession().incrementOutgoingId();

                written += frameBytes;

                // TODO partial consumption
                delivery.setData(null);
                delivery.setDataLength(0);
                delivery.setDone();

                if(delivery.getLink().current() != delivery)
                {
                    transportLink.setDeliveryCount(transportLink.getDeliveryCount().add(UnsignedInteger.ONE));
                    transportLink.setLinkCredit(transportLink.getLinkCredit().subtract(UnsignedInteger.ONE));
                }

                delivery.getLink().decrementQueued();

                delivery = delivery.clearTransportWork();


            }
            else
            {
                delivery = delivery.getTransportWorkNext();
            }
        }
        return written;
    }

    private int processReceiverDisposition(WritableBuffer buffer)
    {
        DeliveryImpl delivery = _connectionEndpoint.getTransportWorkHead();
        int written = 0;
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
                DeliveryState deliveryState = delivery.getLocalState();
                if(deliveryState == Accepted.getInstance())
                {
                    disposition.setState(ACCEPTED);
                }
                else
                {
                    // TODO
                }
                int frameBytes = writeFrame(buffer, delivery.getLink().getSession()
                                                                  .getTransportSession().getLocalChannel(),
                                   disposition, null);
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
        return written;
    }

    private int processReceiverFlow(WritableBuffer buffer)
    {
        EndpointImpl endpoint = _connectionEndpoint.getTransportHead();
        int written = 0;
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
                        int frameBytes = writeFrame(buffer, transportSession.getLocalChannel(), flow, null);
                        written += frameBytes;
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
                        int frameBytes = writeFrame(buffer, transportSession.getLocalChannel(), flow, null);
                        written += frameBytes;
                    }
                }
            }
            endpoint = endpoint.transportNext();
        }
        return written;
    }

    private int processAttach(WritableBuffer buffer)
    {
        EndpointImpl endpoint = _connectionEndpoint.getTransportHead();
        int written = 0;
        while(endpoint != null && buffer.remaining() >= _maxFrameSize)
        {

            if(endpoint instanceof LinkImpl)
            {

                LinkImpl link = (LinkImpl) endpoint;
                TransportLink transportLink = getTransportState(link);
                if(link.getLocalState() == EndpointState.ACTIVE)
                {

                    if( (link.getRemoteState() == EndpointState.ACTIVE
                        && !transportLink.isLocalHandleSet()) || link.getRemoteState() == EndpointState.UNINITIALIZED)
                    {

                        SessionImpl session = link.getSession();
                        TransportSession transportSession = getTransportState(session);
                        UnsignedInteger localHandle = transportSession.allocateLocalHandle();
                        transportLink.setLocalHandle(localHandle);

                        if(link.getRemoteState() == EndpointState.UNINITIALIZED)
                        {
                            transportSession.addHalfOpenLink(transportLink);
                        }

                        Attach attach = new Attach();
                        attach.setHandle(localHandle);
                        attach.setName(transportLink.getName());

                        if(link.getLocalSourceAddress() != null)
                        {
                            Source source = new Source();
                            source.setAddress(link.getLocalSourceAddress());
                            attach.setSource(source);
                        }

                        if(link.getLocalTargetAddress() != null)
                        {
                            Target target = new Target();
                            target.setAddress(link.getLocalTargetAddress());
                            attach.setTarget(target);
                        }

                        attach.setRole(endpoint instanceof ReceiverImpl ? Role.RECEIVER : Role.SENDER);

                        if(link instanceof SenderImpl)
                        {
                            attach.setInitialDeliveryCount(UnsignedInteger.ZERO);
                        }

                        int frameBytes = writeFrame(buffer, transportSession.getLocalChannel(), attach, null);
                        written += frameBytes;
                    }
                }

            }
            endpoint = endpoint.transportNext();
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
        if(_connectionEndpoint.getLocalState() != EndpointState.UNINITIALIZED && !_isOpenSent)
        {
            Open open = new Open();
            open.setContainerId(_connectionEndpoint.getLocalContainerId());
            // TODO - populate;

            _isOpenSent = true;

            return  writeFrame(buffer, 0, open, null);

        }
        return 0;
    }

    private int processBegin(WritableBuffer buffer)
    {
        EndpointImpl endpoint = _connectionEndpoint.getTransportHead();
        int written = 0;
        while(endpoint != null && buffer.remaining() >= _maxFrameSize)
        {
            if(endpoint instanceof SessionImpl)
            {
                SessionImpl session = (SessionImpl) endpoint;
                TransportSession transportSession = getTransportState(session);
                if(session.getLocalState() == EndpointState.ACTIVE)
                {
                    int channelId = allocateLocalChannel();
                    transportSession.setLocalChannel(channelId);
                    _localSessions[channelId] = transportSession;

                    Begin begin = new Begin();


                    if(session.getRemoteState() != EndpointState.UNINITIALIZED)
                    {
                        begin.setRemoteChannel(UnsignedShort.valueOf((short) transportSession.getRemoteChannel()));
                    }
                    begin.setHandleMax(transportSession.getHandleMax());
                    begin.setIncomingWindow(transportSession.getIncomingWindowSize());
                    begin.setOutgoingWindow(transportSession.getOutgoingWindowSize());
                    begin.setNextOutgoingId(transportSession.getNextOutgoingId());

                    written += writeFrame(buffer, channelId, begin, null);
                }
            }
            endpoint = endpoint.transportNext();
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

    private int allocateLocalChannel()
    {
        return 0;  //TODO - Implement
    }

    private int processEnd(WritableBuffer buffer)
    {
        EndpointImpl endpoint = _connectionEndpoint.getTransportHead();
        int written = 0;
        while(endpoint != null && buffer.remaining() >= _maxFrameSize)
        {

            if(endpoint instanceof SessionImpl)
            {

                SessionImpl session = (SessionImpl) endpoint;
                TransportSession transportSession = session.getTransportSession();
                if(session.getLocalState() == EndpointState.CLOSED
                   && transportSession.isLocalChannelSet())
                {

                    int channel = transportSession.getLocalChannel();
                    transportSession.freeLocalChannel();
                    _localSessions[channel] = null;


                    End end = new End();

                    int frameBytes = writeFrame(buffer, channel, end, null);
                    written += frameBytes;
                }

            }
            endpoint = endpoint.transportNext();
        }
        return written;
    }

    private int processClose(WritableBuffer buffer)
    {
        if(_connectionEndpoint.getLocalState() == EndpointState.CLOSED && !_isCloseSent)
        {
            Close close = new Close();

            // TODO - populate;

            _isCloseSent = true;

            return  writeFrame(buffer, 0, close, null);

        }
        return 0;

    }



    private int writeFrame(WritableBuffer buffer, int channel, DescribedType frameBody, ByteBuffer payload)
    {
        int oldPosition = buffer.position();
        buffer.position(buffer.position()+8);
        _encoder.setByteBuffer(buffer);
        _encoder.writeDescribedType(frameBody);

        int payloadSize = Math.min(payload == null ? 0 : payload.remaining(), _maxFrameSize);
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
        _connectionEndpoint.clearTransport();
    }

    //==================================================================================================================
    // handle incoming amqp data


    public void handleOpen(Open open, Binary payload, Integer channel)
    {
        _connectionEndpoint.handleOpen(open);
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

                link.setRemoteState(EndpointState.ACTIVE);
                Source source = (Source) attach.getSource();
                if(source != null)
                {
                    link.setRemoteSourceAddress(source.getAddress());
                }
                Target target = (Target) attach.getTarget();
                if(target != null)
                {
                    link.setRemoteTargetAddress(target.getAddress());
                }

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

            transportSession.getSession().setRemoteState(EndpointState.CLOSED);

        }
    }

    public void handleClose(Close close, Binary payload, Integer channel)
    {
        _connectionEndpoint.setRemoteState(EndpointState.CLOSED);
    }

}
