/*
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
 */
package org.apache.qpid.proton.systemtests.engine;

import static java.util.EnumSet.of;
import static org.apache.qpid.proton.engine.EndpointState.ACTIVE;
import static org.apache.qpid.proton.engine.EndpointState.CLOSED;
import static org.apache.qpid.proton.engine.EndpointState.UNINITIALIZED;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

import org.apache.qpid.proton.Proton;
import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.transport.Close;
import org.apache.qpid.proton.amqp.transport.ErrorCondition;
import org.apache.qpid.proton.amqp.transport.Open;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Endpoint;
import org.apache.qpid.proton.engine.EndpointState;
import org.apache.qpid.proton.engine.Session;
import org.apache.qpid.proton.engine.Transport;
import org.apache.qpid.proton.engine.impl.AmqpFramer;
import org.junit.Ignore;
import org.junit.Test;

/**
 * Implicitly tests both {@link Connection} and {@link Transport} (e.g. for stuff like the AMQP header exchange).
 *
 * TODO test that the connection properties, connection capability, and error info maps have keys that are exclusively of type Symbol.
 */
public class ConnectionTest
{
    private static final String SERVER_CONTAINER = "serverContainer";
    private static final String CLIENT_CONTAINER = "clientContainer";

    private final Transport _clientTransport = Proton.transport();
    private final Transport _serverTransport = Proton.transport();

    private final TransportPumper _pumper = new TransportPumper(_clientTransport, _serverTransport);

    private final Connection _clientConnection = Proton.connection();
    private final Connection _serverConnection = Proton.connection();

    private final AmqpFramer _framer = new AmqpFramer();

    // 2.4.1 Opening A Connection

    /** */
    @Test
    public void testOpenConnection()
    {
        _pumper.pumpAll();

        bindAndOpenConnections();
    }


    /** Container id is a mandatory field so this should cause an error */
    @Test
    public void testReceiptOfOpenWithoutContainerId_causesTODO()
    {
        _pumper.pumpAll();

        Open openWithoutContainerId = new Open();
        byte[] openFrameBuffer = _framer.generateFrame(0, openWithoutContainerId);

        int serverConsumed = _serverTransport.input(openFrameBuffer, 0, openFrameBuffer.length);
        assertEquals(openFrameBuffer.length, serverConsumed);
        assertEquals(_serverTransport.capacity(), Transport.END_OF_STREAM);
    }

    /**
     * "Prior to any explicit negotiation, the maximum frame size is 512 (MIN-MAX-FRAME-SIZE) and the maximum channel number is 0"
     * */
    @Test
    public void testReceiptOfOpenExactlyDefaultMaximumFrameSize()
    {
        _pumper.pumpAll();

        _serverTransport.bind(_serverConnection);
        assertEnpointState(_serverConnection, UNINITIALIZED, UNINITIALIZED);

        // containerId and extended header sized to give an open frame
        // exactly 512 bytes in length.
        String containerId = "12345678";
        int extendedHeaderSize = 122 * 4;

        Open open = new Open();
        open.setContainerId(containerId);
        byte[] openFrameBuffer = _framer.generateFrame(0, new byte[extendedHeaderSize], open);
        assertEquals("Test requires a frame of size MIN_MAX_FRAME_SIZE",
                Transport.MIN_MAX_FRAME_SIZE, openFrameBuffer.length);

        int serverConsumed = _serverTransport.input(openFrameBuffer, 0, openFrameBuffer.length);
        assertEquals(openFrameBuffer.length, serverConsumed);

        // Verify that the server has seen the Open arrive
        assertEnpointState(_serverConnection, UNINITIALIZED, ACTIVE);
        assertEquals(containerId, _serverConnection.getRemoteContainer());
    }

    /**
     * "Prior to any explicit negotiation, the maximum frame size is 512 (MIN-MAX-FRAME-SIZE) and the maximum channel number is 0"
     */
    @Test
    public void testReceiptOfOpenBiggerThanDefaultMaximumFrameSize_causesTODO()
    {
        _pumper.pumpAll();

        _serverTransport.bind(_serverConnection);
        assertEnpointState(_serverConnection, UNINITIALIZED, UNINITIALIZED);

        // containerId and extended header sized to give an open frame
        // 1 byte larger the than 512 bytes permitted before negotiation by the AMQP spec.

        String containerId = "123456789";
        int extendedHeaderSize = 122 * 4;

        Open bigOpen = new Open();
        bigOpen.setContainerId(containerId);
        byte[] openFrameBuffer = _framer.generateFrame(0, new byte[extendedHeaderSize], bigOpen);
        assertEquals("Test requires a frame of size MIN_MAX_FRAME_SIZE + 1",
                Transport.MIN_MAX_FRAME_SIZE + 1, openFrameBuffer.length);

        int serverConsumed = _serverTransport.input(openFrameBuffer, 0, openFrameBuffer.length);
        assertEquals(openFrameBuffer.length, serverConsumed);

        // TODO server should indicate error but currently both implementations currently process
        // the larger frames.   The following assertions should fail but currently pass.
        assertEnpointState(_serverConnection, UNINITIALIZED, ACTIVE);
        assertNotNull(_serverConnection.getRemoteContainer());
    }

    @Test
    public void testReceiptOfSecondOpen_causesTODO()
    {
        bindAndOpenConnections();

        Open secondOpen  = new Open(); // erroneous
        secondOpen.setContainerId("secondOpen");
        byte[] openFrameBuffer = _framer.generateFrame(0, secondOpen);

        int serverConsumed = _serverTransport.input(openFrameBuffer, 0, openFrameBuffer.length);
        assertEquals(openFrameBuffer.length, serverConsumed);

        // TODO server should indicate error but currently both implementation currently
        // allow this condition
    }

    /** "each peer MUST send an open frame before sending any other frames"
     *
     * @see ConnectionTest#testReceiptOfCloseBeforeOpen_causesTODO()
     */
    public void testReceiptOfIntialFrameOtherThanOpen_causesTODO()
    {
    }

    /**
     * 2.4.5 "Implementations MUST be prepared to handle empty frames arriving on any valid channel"
     *
     * TODO consider moving to {@link TransportTest} once we have a less Connection-centric way of
     * checking health than calling {@link #bindAndOpenConnections()}
     */
    @Test
    public void testReceiptOfInitialEmptyFrame_isAllowed()
    {
        _pumper.pumpAll();

        byte[] emptyFrame = _framer.createEmptyFrame(0);
        int bytesConsumed = _serverTransport.input(emptyFrame, 0, emptyFrame.length);
        assertEquals(emptyFrame.length, bytesConsumed);

        bindAndOpenConnections();
    }


    /** "The open frame can only be sent on channel 0" */
    @Test
    @Ignore("Reinstate once it is agreed how error condition will be reported to user of API")
    public void testReceiptOfOpenOnNonZeroChannelNumber_causesTODO()
    {
        _pumper.pumpAll();

        Open open = new Open();
        open.setContainerId(SERVER_CONTAINER);

        int nonZeroChannelId = 1;
        byte[] buf = _framer.generateFrame(nonZeroChannelId, open);
        int rv = _serverTransport.input(buf, 0, buf.length);
        // TODO server should indicate error
    }


    /**
     * "After sending the open frame and reading its partner's open frame a peer MUST operate within
     * mutually acceptable limitations from this point forward"
     * see 2.7.1 "A peer that receives an oversized frame MUST close the connection with the framing-error error-code"
     */
    public void testReceiptOfFrameLargerThanAgreedMaximumSize_causesTODO()
    {
    }

    public void testThatSentFramesAreWithinMaximumSizeLimit()
    {
    }

    // 2.4.2 Pipelined Open

    /** test that the other peer accepts the pipelined frames and creates an open connection */
    @Test
    public void testReceiptOfOpenUsingPipelining()
    {
        _clientConnection.setContainer(CLIENT_CONTAINER);
        _clientTransport.bind(_clientConnection);
        _clientConnection.open();

        _serverTransport.bind(_serverConnection);

        // when pipelining, we delay pumping until the connection is both bound and opened
        _pumper.pumpOnceFromClientToServer();

        assertEnpointState(_clientConnection, ACTIVE, UNINITIALIZED);
        assertEnpointState(_serverConnection, UNINITIALIZED, ACTIVE);
    }


    /** test that the other peer accepts the pipelined frames and creates an already-closed connection */
    @Test
    public void testReceiptOfOpenThenCloseUsingPipelining()
    {
        _clientConnection.setContainer(CLIENT_CONTAINER);
        _clientTransport.bind(_clientConnection);
        _clientConnection.open();
        _clientConnection.close();

        _serverTransport.bind(_serverConnection);
        _pumper.pumpOnceFromClientToServer();

        assertEnpointState(_clientConnection, CLOSED, UNINITIALIZED);
        assertEnpointState(_serverConnection, UNINITIALIZED, CLOSED);
    }

    /**
     * Similar to {@link #testReceiptOfOpenUsingPipelining()} but opens both ends of the connection
     * so we can actually use it.
     */
    @Test
    public void testOpenConnectionUsingPipelining()
    {
        _clientConnection.setContainer(CLIENT_CONTAINER);
        _clientTransport.bind(_clientConnection);
        _clientConnection.open();


        _serverConnection.setContainer(SERVER_CONTAINER);
        _serverTransport.bind(_serverConnection);
        _serverConnection.open();

        _pumper.pumpAll();

        assertEnpointState(_clientConnection, ACTIVE, ACTIVE);
        assertEnpointState(_serverConnection, ACTIVE, ACTIVE);

        assertConnectionIsUsable();
    }

    // 2.4.3 Closing A Connection and 2.7.9 Close

    /**
     * "each peer MUST write a close frame"
     * Omits the optional error field
     */
    @Test
    public void testCloseConnection()
    {
        bindAndOpenConnections();

        assertEnpointState(_clientConnection, ACTIVE, ACTIVE);
        assertEnpointState(_serverConnection, ACTIVE, ACTIVE);

        _clientConnection.close();

        assertEnpointState(_clientConnection, CLOSED, ACTIVE);
        assertEnpointState(_serverConnection, ACTIVE, ACTIVE);

        _pumper.pumpAll();

        assertEnpointState(_clientConnection, CLOSED, ACTIVE);
        assertEnpointState(_serverConnection, ACTIVE, CLOSED);

        _serverConnection.close();

        assertEnpointState(_clientConnection, CLOSED, ACTIVE);
        assertEnpointState(_serverConnection, CLOSED, CLOSED);

        _pumper.pumpAll();

        assertEnpointState(_clientConnection, CLOSED, CLOSED);
        assertEnpointState(_serverConnection, CLOSED, CLOSED);
    }

    /**
     * "each peer MUST write a close frame with a code indicating the reason for closing"
     * Also see 2.8.16 Connection Error
     */
    @Test
    @SuppressWarnings({ "rawtypes", "unchecked" })
    public void testCloseConnectionWithErrorCode_causesCloseFrameContainingErrorCodeToBeSent()
    {
        bindAndOpenConnections();

        /*
         * TODO javadoc for {@link Connection#getCondition()} states null is returned if there is no condition,
         * this differs from the implementation of both Proton-c and Proton-j.
         */
        assertNull(_clientConnection.getCondition().getCondition());
        assertNull(_serverConnection.getCondition().getCondition());

        assertNull(_clientConnection.getRemoteCondition().getCondition());
        assertNull(_serverConnection.getRemoteCondition().getCondition());

        ErrorCondition clientErrorCondition = new ErrorCondition(Symbol.getSymbol("myerror"), "mydescription");
        Map info = new HashMap();
        info.put(Symbol.getSymbol("simplevalue"), "value");
        info.put(Symbol.getSymbol("list"), Arrays.asList("e1", "e2", "e3"));
        clientErrorCondition.setInfo(info);
        _clientConnection.setCondition(clientErrorCondition);

        _clientConnection.close();
        _pumper.pumpAll();

        assertEquals(clientErrorCondition, _serverConnection.getRemoteCondition());
        assertNull(_serverConnection.getCondition().getCondition());
    }

    /**
     * "each peer MUST write a close frame with a code indicating the reason for closing"
     */
    public void testReceiptOfConnectionCloseContainingErrorCode_allowsErrorCodeToBeObserved()
    {
    }

    /**
     * A test for when the connection close frame contains a session error
     * rather than a connection error. This is allowed by the spec.
     */
    public void testReceiptOfConnectionCloseContainingNonConnectionErrorCode_causesTODO()
    {
    }

    /** "This frame MUST be the last thing ever written onto a connection." */
    public void testUsingProtonAfterClosingConnection_doesntCauseFrameToBeSent()
    {
    }

    /** "This frame MUST be the last thing ever written onto a connection." */
    public void testReceiptOfFrameAfterClose_causesTODO()
    {
    }

    /** "A close frame MAY be received on any channel up to the maximum channel number negotiated in open" */
    public void testReceiptOfCloseOnNonZeroChannelNumber_causesHappyPathTODO()
    {
    }

    /**
     * "each peer MUST send an open frame before sending any other frames"
     */
    @Test
    public void testReceiptOfCloseBeforeOpen_causesTODO()
    {
        _pumper.pumpAll();

        Close surprisingClose = new Close();

        byte[] buf = _framer.generateFrame(0, surprisingClose);
        _serverTransport.input(buf, 0, buf.length);

        // TODO server should indicate error
    }

    // 2.4.4 Simultaneous Close

    /** "both endpoints MAY simultaneously" */
    public void testPeersCloseConnectionSimultaneously()
    {
    }

    // 2.4.5 Idle Timeout Of A Connection

    public void testReceiptOfFrame_preventsIdleTimeoutOccurring()
    {
    }

    /** "If the threshold is exceeded, then a peer SHOULD try to gracefully close the connection using a close frame with an error explaining why" */
    public void testReceiptOfFrameTooLate_causedIdleTimeoutToOccur()
    {
    }

    /** "Each peer has its own (independent) idle timeout." */
    public void testPeersWithDifferentIdleTimeouts_timeOutAtTheCorrectTimes()
    {
    }

    /**
     * "If the value is not set, then the sender does not have an idle time-out. However,
     * senders doing this SHOULD be aware that implementations MAY choose to use an internal default
     * to efficiently manage a peer's resources."
     */
    public void testReceiptOfFrameWithZeroIdleTimeout_causesNoIdleFramesToBeSent()
    {
    }

    /**
     * "If a peer can not, for any reason support a proposed idle timeout,
     * then it SHOULD close the connection using a close frame with an error explaining why"
     */
    public void testReceiptOfOpenWithUnsupportedTimeout_causesCloseWithError()
    {
    }

    /**
     * implementations ... MUST use channel 0 if a maximum channel number has not yet been negotiated
     * (i.e., before an open frame has been received)
     */
    public void testReceiptOfEmptyFrameOnNonZeroChannelBeforeMaximumChannelsNegotiated_causesTODO()
    {
    }


    // 2.4.7 State transitions

    /**
     * The DISCARDING state is a variant of the CLOSE_SENT state where the close is triggered by an error.
     * In this case any incoming frames on the connection MUST be silently discarded until the peer's close frame is received
     */
    public void testReceiptOfFrameWhenInDiscardingState_isIgnored()
    {
    }

    // 2.7.1 Open

    public void testReceiptOfOpen_containerCanBeRetrieved()
    {
    }

    /**
     * The spec says:
     * "If no hostname is provided the receiving peer SHOULD select a default based on its own configuration"
     * but Proton's Engine layer does not do any defaulting - this is the responsibility
     * of other layers e.g. Messenger or Driver.
     */
    public void testReceiptOfOpenWithoutHostname_nullHostnameIsRetrieved()
    {
    }

    public void testReceiptOfOpenWithHostname_hostnameCanBeRetrieved()
    {
    }

    /**
     * "Both peers MUST accept frames of up to 512 (MIN-MAX-FRAME-SIZE) octets."
     */
    public void testReceiptOfOpenWithMaximumFramesizeLowerThanMinMaxFrameSize_causesTODO()
    {
    }

    public void testInitiatingPeerAndReceivingPeerUseDifferentMaxFrameSizes()
    {
    }

    public void testReceiptOfSessionBeginThatBreaksChannelMax_causesTODO()
    {
    }

    public void testCreationOfSessionThatBreaksChannelMax_causesTODO()
    {
    }

    public void testOpenConnectionWithPeersUsingUnequalChannelMax_enforcesLowerOfTwoValues()
    {
    }

    public void testOpenConnectionWithOnePeerUsingUnsetChannelMax_enforcesTheSetValue()
    {
    }

    public void testReceiptOfBeginWithInUseChannelId_causesTODO()
    {
    }

    /** "If a session is locally initiated, the remote-channel MUST NOT be set." */
    public void testReceiptOfUnsolicitedBeginWithChannelId_causesTODO()
    {
    }

    /**
     * "When an endpoint responds to a remotely initiated session, the remote-channel MUST be set
     * to the channel on which the remote session sent the begin."
     */
    public void testThatBeginResponseContainsChannelId()
    {
    }

    /**
     * I imagine we will want to begin ChannelMax number of sessions, then end
     * a session from the 'middle'.  Then check we are correctly begin a new
     * channel.
     */
    public void testEnd_channelNumberAvailableForReuse()
    {
    }

    public void testReceiptOfOpenWithOutgoingLocales_outgoingLocalesCanBeRetrieved()
    {
    }

    /** "A null value or an empty list implies that only en-US is supported. " */
    public void testReceiptOfOpenWithNullOutgoingLocales_defaultOutgoingLocaleCanBeRetrieved()
    {
    }

    /** "A null value or an empty list implies that only en-US is supported. " */
    public void testReceiptOfOpenWithEmptyListOfOutgoingLocales_defaultOutgoingLocaleCanBeRetrieved()
    {
    }

    public void testReceiptOfOpenWithIncomingLocales_incomingLocalesCanBeRetrieved()
    {
    }

    /** "A null value or an empty list implies that only en-US is supported. " */
    public void testReceiptOfOpenWithNullIncomingLocales_defaultIncomingLocaleCanBeRetrieved()
    {
    }

    /** "A null value or an empty list implies that only en-US is supported. " */
    public void testReceiptOfOpenWithEmptyListOfIncomingLocales_defaultIncomingLocaleCanBeRetrieved()
    {
    }

    // TODO It seems that currently Proton-j merely exposes the remote capabilities to
    // the user and is seems to be a end-user responsibility to enforce "If the receiver of the
    // offered-capabilities requires an extension capability which is not present in the
    // offered-capability list then it MUST close the connection.".  However, i wonder if this
    // is an omission -- surely Proton could valid that request desirable capabilities are
    // offered by the remote???

    public void testReceiptOfOpenWithOfferedCapabilities_offeredCapabilitiesCanBeRetrieved()
    {
    }

    public void testReceiptOfOpenWithDesiredCapabilities_desiredCapabilitiesCanBeRetrieved()
    {
    }

    public void testReceiptOfOpenWithProperties_propertiesCanBeRetrieved()
    {
    }

    // Transport/Connection related api-inspired tests

    /**
     * TODO is there a limit on the number of connections?
     * Also try closing them in a different order to their creation.
     */
    public void testCreateMultipleConnections()
    {
    }

    public void testBindTwoConnectionsToATransport_causesTODO()
    {
    }

    public void testBindAConnectionToTwoTransports_causesTODO()
    {
    }

    /**
     * TODO possibly try to bind this "opened" connection too if it doesn't go pop before this.
     */
    public void testOpenBeforeBind_causesTODO()
    {
    }

    public void testOpenTwice_throwsExceptionTODO()
    {
    }

    public void testOpenAfterClose_throwsExceptionTODO()
    {
    }

    // Connection.java-related api-inspired tests

    /**
     * also test that the session appears in the connection's session list
     */
    public void testCreateSession()
    {
    }

    public void testSessionHeadWhenNoSessionsExist_returnsNull()
    {
    }

    public void testSessionHead_returnsSessionsMatchingCriteria()
    {
    }

    public void testLinkHeadWhenNoLinksExist_returnsNull()
    {
    }

    public void testLinkHead_returnsLinksMatchingCriteria()
    {
    }

    public void testGetWorkHeadWhenNoWork_returnsNull()
    {
    }

    public void testGetWorkHeadWhenOneDeliveryIsPending_returnsTheDelivery()
    {
    }

    /**
     * use a name that is longer than the limit of AMQShortString
     */
    public void testSetContainerWithLongName_isAllowed()
    {
    }

    public void testSetContainerWithNullName_throwsException()
    {
    }

    public void testSetContainerWithEmptyName_throwsException()
    {
    }

    public void testSetContainerAfterOpeningConnection_throwsExceptionTODO()
    {
    }

    public void testOpenWithoutContainerName_throwsExceptionTODO()
    {
    }

    public void testGetRemoteContainerBeforeOpen_returnsNull()
    {
    }

    public void testGetRemoteContainerBeforeReceiptOfOpen_returnsNull()
    {
    }

    public void testSetHostnameWithLongName_isAllowed()
    {
    }

    /**
     * Proton does not require the conventional foo.bar.com format for hostnames.
     */
    public void testSetHostnameWithNonstandardName_isAllowed()
    {
    }

    public void testSetHostnameAfterOpeningConnection_throwsExceptionTODO()
    {
    }

    public void testSetOfferedCapabilitiesAfterOpeningConnection_throwsExceptionTODO()
    {
    }

    public void testSetDesiredCapabilitiesAfterOpeningConnection_throwsExceptionTODO()
    {
    }

    public void testSetPropertiesAfterOpeningConnection_throwsExceptionTODO()
    {
    }

    // Endpoint api-inspired tests

    public void testGetLocalStateBeforeOpen_returnsUninitialised()
    {
    }

    public void testGetLocalStateAfterClose_returnsClosed()
    {
    }

    public void testGetRemoteStateBeforeReceiptOfOpen_returnsUninitialised()
    {
    }

    public void testGetRemoteStateAfterReceiptOfClose_returnsClosed()
    {
    }

    public void testFree_isAllowed()
    {
    }

    public void testSetContext_contextCanBeRetrieved()
    {
    }

    public void testGetContextWithoutSettingContext_returnsNull()
    {
    }

    private void assertConnectionIsUsable()
    {
        Session clientSesion = _clientConnection.session();
        clientSesion.open();
        _pumper.pumpAll();

        Session serverSession = _serverConnection.sessionHead(of(UNINITIALIZED), of(ACTIVE));
        serverSession.open();
        _pumper.pumpAll();

        assertEnpointState(clientSesion, ACTIVE, ACTIVE);
        assertEnpointState(serverSession, ACTIVE, ACTIVE);
    }

    private void bindAndOpenConnections()
    {
        // TODO should we be checking local and remote error conditions as part of this?

        _clientConnection.setContainer(CLIENT_CONTAINER);
        _serverConnection.setContainer(SERVER_CONTAINER);

        assertEnpointState(_clientConnection, UNINITIALIZED, UNINITIALIZED);
        assertEnpointState(_serverConnection, UNINITIALIZED, UNINITIALIZED);

        _clientTransport.bind(_clientConnection);
        _serverTransport.bind(_serverConnection);

        _clientConnection.open();

        assertEnpointState(_clientConnection, ACTIVE, UNINITIALIZED);
        assertEnpointState(_serverConnection, UNINITIALIZED, UNINITIALIZED);

        _pumper.pumpAll();

        assertEnpointState(_clientConnection, ACTIVE, UNINITIALIZED);
        assertEnpointState(_serverConnection, UNINITIALIZED, ACTIVE);

        _serverConnection.open();

        assertEnpointState(_clientConnection, ACTIVE, UNINITIALIZED);
        assertEnpointState(_serverConnection, ACTIVE, ACTIVE);

        _pumper.pumpAll();

        assertEnpointState(_clientConnection, ACTIVE, ACTIVE);
        assertEnpointState(_serverConnection, ACTIVE, ACTIVE);
    }

    private void assertEnpointState(Endpoint endpoint, EndpointState localState, EndpointState remoteState)
    {
        assertEquals("Unexpected local state", localState, endpoint.getLocalState());
        assertEquals("Unexpected remote state", remoteState, endpoint.getRemoteState());
    }
}
