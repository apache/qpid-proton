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

import org.apache.qpid.proton.codec.DecoderImpl;
import org.apache.qpid.proton.codec.EncoderImpl;
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

    private int _headerWritten;
    private TransportSession[] _remoteSessions;
    private TransportSession[] _localSessions;

    private final FrameParser _frameParser;

    private Map<SessionImpl, TransportSession> _transportSessionState = new HashMap<SessionImpl, TransportSession>();
    private Map<LinkImpl, TransportLink> _transportLinkState = new HashMap<LinkImpl, TransportLink>();


    private DecoderImpl _decoder = new DecoderImpl();
    private EncoderImpl _encoder = new EncoderImpl(_decoder);

    private int _maxFrameSize = 16 * 1024;

    {
        AMQPDefinedTypes.registerAllTypes(_decoder);
    }

    public TransportImpl(Connection connectionEndpoint)
    {
        _connectionEndpoint = (ConnectionImpl) connectionEndpoint;
        _localSessions = new TransportSession[_connectionEndpoint.getMaxChannels()+1];
        _remoteSessions = new TransportSession[_connectionEndpoint.getMaxChannels()+1];
        _frameParser = new FrameParser(this);
    }

    public int input(byte[] bytes, int offset, int length)
    {
        return _frameParser.input(bytes, offset, length);
    }

    //==================================================================================================================
    // Process model state to generate output

    public int output(byte[] bytes, final int offset, final int size)
    {

        int written = 0;

        written += processHeader(bytes, offset);
        written += processOpen(bytes, offset + written, size - written);
        written += processBegin(bytes, offset + written, size - written);
        written += processAttach(bytes, offset + written, size - written);
        written += processReceiverFlow(bytes, offset + written, size - written);
        written += processReceiverDisposition(bytes, offset + written, size - written);
        written += processMessageData(bytes, offset + written, size - written);
        written += processSenderDisposition(bytes, offset + written, size - written);
        written += processSenderFlow(bytes, offset + written, size - written);
        written += processDetach(bytes, offset + written, size - written);
        written += processEnd(bytes, offset+written, size-written);
        written += processClose(bytes, offset+written, size-written);

        if(size - written > _maxFrameSize)
        {
            clearInterestList();
            clearTransportWorkList();
        }

        return written;
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

    private int processDetach(byte[] bytes, int offset, int length)
    {
        EndpointImpl endpoint = _connectionEndpoint.getTransportHead();
        int written = 0;
        while(endpoint != null && length >= _maxFrameSize)
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


                    int frameBytes = writeFrame(bytes, offset, length, transportSession.getLocalChannel(), detach, null);
                    written += frameBytes;
                    offset += frameBytes;
                    length -= frameBytes;
                }

            }
            endpoint = endpoint.getNext();
        }
        return written;
    }

    private int processSenderFlow(byte[] bytes, int offset, int length)
    {
        return 0;  //TODO - Implement
    }

    private int processSenderDisposition(byte[] bytes, int offset, int length)
    {
        return 0;  //TODO - Implement
    }

    private int processMessageData(byte[] bytes, int offset, int length)
    {
        DeliveryImpl delivery = _connectionEndpoint.getTransportWorkHead();
        int written = 0;
        while(delivery != null && length >= _maxFrameSize)
        {
            if((delivery.getLink() instanceof SenderImpl))
            {
                SenderImpl sender = (SenderImpl) delivery.getLink();



                TransportLink transportLink = sender.getTransportLink();
                UnsignedInteger deliveryId = transportLink.getDeliveryCount();
                transportLink.setDeliveryCount(deliveryId.add(UnsignedInteger.ONE));
                TransportDelivery transportDelivery = new TransportDelivery(deliveryId, delivery, transportLink);


                Transfer transfer = new Transfer();
                transfer.setDeliveryId(deliveryId);
                transfer.setDeliveryTag(new Binary(delivery.getTag()));
                transfer.setHandle(transportLink.getLocalHandle());
                transfer.setMessageFormat(UnsignedInteger.ZERO);

                int frameBytes = writeFrame(bytes, offset, length,
                                            sender.getSession().getTransportSession().getLocalChannel(),
                                            transfer, null);
                written += frameBytes;
                offset += frameBytes;
                length -= frameBytes;

            }
            delivery = delivery.getTransportWorkNext();
        }
        return written;
    }

    private int processReceiverDisposition(byte[] bytes, int offset, int length)
    {
        DeliveryImpl delivery = _connectionEndpoint.getTransportWorkHead();
        int written = 0;
        while(delivery != null && length >= _maxFrameSize)
        {
            if((delivery.getLink() instanceof ReceiverImpl) && delivery.isLocalStateChange())
            {
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
                int frameBytes = writeFrame(bytes, offset, length, delivery.getLink().getSession()
                                                                  .getTransportSession().getLocalChannel(),
                                   disposition, null);
                written += frameBytes;
                offset += frameBytes;
                length -= frameBytes;
            }
            delivery = delivery.getTransportWorkNext();
        }
        return written;
    }

    private int processReceiverFlow(byte[] bytes, int offset, int length)
    {
        EndpointImpl endpoint = _connectionEndpoint.getTransportHead();
        int written = 0;
        while(endpoint != null && length >= _maxFrameSize)
        {

            if(endpoint instanceof ReceiverImpl)
            {

                ReceiverImpl receiver = (ReceiverImpl) endpoint;
                TransportLink transportLink = getTransportState(receiver);
                TransportSession transportSession = getTransportState(receiver.getSession());

                if(receiver.getLocalState() == EndpointState.ACTIVE)
                {
                    int credits = receiver.clearCredits();
                    if(credits != 0)
                    {
                        transportLink.addCredit(credits);
                        Flow flow = new Flow();
                        flow.setHandle(transportLink.getLocalHandle());
                        flow.setIncomingWindow(transportSession.getIncomingWindowSize());
                        flow.setOutgoingWindow(transportSession.getOutgoingWindowSize());
                        flow.setDeliveryCount(transportLink.getDeliveryCount());
                        flow.setLinkCredit(transportLink.getLinkCredit());
                        flow.setNextOutgoingId(transportSession.getNextOutgoingId());
                        int frameBytes = writeFrame(bytes, offset, length, transportSession.getLocalChannel(), flow, null);
                        written += frameBytes;
                        offset += frameBytes;
                        length -= frameBytes;
                    }
                }
            }
            endpoint = endpoint.getNext();
        }
        return written;
    }

    private int processAttach(byte[] bytes, int offset, int length)
    {
        EndpointImpl endpoint = _connectionEndpoint.getTransportHead();
        int written = 0;
        while(endpoint != null && length >= _maxFrameSize)
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

                        int frameBytes = writeFrame(bytes, offset, length, transportSession.getLocalChannel(), attach, null);
                        written += frameBytes;
                        offset += frameBytes;
                        length -= frameBytes;
                    }
                }

            }
            endpoint = endpoint.getNext();
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

    private int processHeader(byte[] bytes, int offset)
    {
        int headerWritten = 0;
        while(_headerWritten < HEADER.length)
        {
            bytes[offset+(headerWritten++)] = HEADER[_headerWritten++];
        }
        return headerWritten;
    }

    private int processOpen(byte[] bytes, int offset, int length)
    {
        if(_connectionEndpoint.getLocalState() != EndpointState.UNINITIALIZED && !_isOpenSent)
        {
            Open open = new Open();
            open.setContainerId(_connectionEndpoint.getLocalContainerId());
            // TODO - populate;

            _isOpenSent = true;

            return  writeFrame(bytes, offset, length, 0, open, null);

        }
        return 0;
    }

    private int processBegin(byte[] bytes, final int offset, final int length)
    {
        EndpointImpl endpoint = _connectionEndpoint.getTransportHead();
        int written = 0;
        while(endpoint != null && length >= _maxFrameSize)
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

                    written += writeFrame(bytes, offset, length, channelId, begin, null);
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

    private int processEnd(byte[] bytes, int offset, int length)
    {
        EndpointImpl endpoint = _connectionEndpoint.getTransportHead();
        int written = 0;
        while(endpoint != null && length >= _maxFrameSize)
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

                    int frameBytes = writeFrame(bytes, offset, length, channel, end, null);
                    written += frameBytes;
                    offset += frameBytes;
                    length -= frameBytes;
                }

            }
            endpoint = endpoint.getNext();
        }
        return written;
    }

    private int processClose(byte[] bytes, final int offset, final int length)
    {
        if(_connectionEndpoint.getLocalState() == EndpointState.CLOSED && !_isCloseSent)
        {
            Close close = new Close();

            // TODO - populate;

            _isCloseSent = true;

            return  writeFrame(bytes, offset, length, 0, close, null);

        }
        return 0;

    }



    private int writeFrame(byte[] bytes, int offset, int size, int channel, DescribedType frameBody, ByteBuffer payload)
    {
        ByteBuffer buf = ByteBuffer.wrap(bytes, offset+8, size-8);
        int oldPosition = buf.position();
        _encoder.setByteBuffer(buf);
        _encoder.writeDescribedType(frameBody);

        int frameSize = 8 + buf.position() - oldPosition;
        bytes[offset] = (byte) ((frameSize>>24) & 0xFF);
        bytes[offset+1] = (byte) ((frameSize>>16) & 0xFF);
        bytes[offset+2] = (byte) ((frameSize>>8) & 0xFF);
        bytes[offset+3] = (byte) (frameSize & 0xFF);
        bytes[offset+4] = (byte) 2;
        bytes[offset+6] =  (byte) ((channel>>8) & 0xFF);
        bytes[offset+7] =  (byte) (channel & 0xFF);


        return frameSize;
    }

    //==================================================================================================================

    @Override
    protected ConnectionImpl getConnectionImpl()
    {
        return _connectionEndpoint;
    }

    public void destroy()
    {
        super.destroy();
        _connectionEndpoint.clearTransport();
    }

    //==================================================================================================================
    // handle incoming amqp data


    public void handleOpen(Open open, Binary payload, Integer channel)
    {
        System.out.println("CH["+channel+"] : " + open);
        _connectionEndpoint.handleOpen(open);
    }

    public void handleBegin(Begin begin, Binary payload, Integer channel)
    {
        System.out.println("CH["+channel+"] : " + begin);
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
            _remoteSessions[channel] = transportSession;


        }

    }

    public void handleAttach(Attach attach, Binary payload, Integer channel)
    {
        System.out.println("CH["+channel+"] : " + attach);
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
        System.out.println("CH["+channel+"] : " + flow);
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
        System.out.println("CH["+channel+"] : " + transfer);
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

    public void handleDisposition(Disposition disposition, Binary payload, Integer context)
    {
        //To change body of implemented methods use File | Settings | File Templates.
    }

    public void handleDetach(Detach detach, Binary payload, Integer channel)
    {
        System.out.println("CH["+channel+"] : " + detach);
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
        System.out.println("CH["+channel+"] : " + end);
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
        System.out.println("CH["+channel+"] : " + close);
        _connectionEndpoint.setRemoteState(EndpointState.CLOSED);
    }

}