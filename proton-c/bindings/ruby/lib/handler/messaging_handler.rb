#--
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#++

module Qpid::Proton::Handler

  # A general purpose handler that simplifies processing events.
  #
  # @example
  #
  class MessagingHandler < Qpid::Proton::BaseHandler

    attr_reader :handlers

    # Creates a new instance.
    #
    # @param [Fixnum] prefetch
    # @param [Boolean] auto_accept
    # @param [Boolean] auto_settle
    # @param [Boolean] peer_close_is_error
    #
    def initialize(prefetch = 10, auto_accept = true, auto_settle = true, peer_close_is_error = false)
      @handlers = Array.new
      @handlers << CFlowController.new(prefetch) unless prefetch.zero?
      @handlers << EndpointStateHandler.new(peer_close_is_error, self)
      @handlers << IncomingMessageHandler.new(auto_accept, self)
      @handlers << OutgoingMessageHandler.new(auto_settle,self)
    end

    # Called when the peer closes the connection with an error condition.
    #
    # @param event [Qpid::Proton::Event::Event] The event.
    #
    def on_connection_error(event)
      EndpointStateHandler.print_error(event.connection, "connection")
    end

      # Called when the peer closes the session with an error condition.
      #
      # @param event [Qpid:Proton::Event::Event] The event.
      #
    def on_session_error(event)
      EndpointStateHandler.print_error(event.session, "session")
      event.connection.close
    end

    # Called when the peer closes the link with an error condition.
    #
    # @param event [Qpid::Proton::Event::Event] The event.
    #
    def on_link_error(event)
      EndpointStateHandler.print_error(event.link, "link")
      event.connection.close
    end

    # Called when the event loop starts.
    #
    # @param event [Qpid::Proton::Event::Event] The event.
    #
    def on_reactor_init(event)
      self.on_start(event)
    end

    # Called when the event loop starts.
    #
    # This method needs to be overridden.
    #
    # @param event [Qpid::Proton::Event::Event] The event.
    #
    def on_start(event)
    end

    # Called when the connection is closed.
    #
    # This method needs to be overridden.
    #
    # @param event [Qpid::Proton::Event::Event] The event.
    #
    def on_connection_closed(event)
    end

    # Called when the session is closed.
    #
    # This method needs to be overridden.
    #
    # @param event [Qpid::Proton::Event::Event] The event.
    #
    def on_session_closed(event)
    end

    # Called when the link is closed.
    #
    # This method needs to be overridden.
    #
    # @param event [Qpid::Proton::Event::Event] The event.
    #
    def on_link_closed(event)
    end

    # Called when the peer initiates the closing of the connection.
    #
    # This method needs to be overridden.
    #
    # @param event [Qpid::Proton::Event::Event] The event.
    #
    def on_connection_closing(event)
    end

    # Called when the peer initiates the closing of the session.
    #
    # This method needs to be overridden.
    #
    # @param event [Qpid::Proton::Event::Event] The event.
    #
    def on_session_closing(event)
    end

    # Called when the peer initiates the closing of the link.
    #
    # This method needs to be overridden.
    #
    # @param event [Qpid::Proton::Event::Event] The event.
    #
    def on_link_closing(event)
    end

    # Called when the socket is disconnected.
    #
    # This method needs to be overridden.
    #
    # @param event [Qpid::Proton::Event::Event] The event.
    #
    def on_disconnected(event)
    end

    # Called when the sender link has credit and messages can therefore
    # be transferred.
    #
    # This method needs to be overridden.
    #
    # @param event [Qpid::Proton::Event::Event] The event.
    #
    def on_sendable(event)
    end

    # Called when the remote peer accepts an outgoing message.
    #
    # This method needs to be overridden.
    #
    # @param event [Qpid::Proton::Event::Event] The event.
    #
    def on_accepted(event)
    end

    # Called when the remote peer rejects an outgoing message.
    #
    # This method needs to be overridden.
    #
    # @param event [Qpid::Proton::Event::Event] The event.
    #
    def on_rejected(event)
    end

    # Called when the remote peer releases an outgoing message.
    #
    # Note that this may be in response to either the RELEASE or
    # MODIFIED state as defined by the AMPQ specification.
    #
    # This method needs to be overridden.
    #
    # @param event [Qpid::Proton::Event::Event] The event.
    #
    def on_released(event)
    end

    # Called when the remote peer has settled hte outgoing message.
    #
    # This is the point at which it should never be retransmitted.
    #
    # This method needs to be overridden.
    #
    # @param event [Qpid::Proton::Event::Event] The event.
    #
    def on_settled(event)
    end

    # Called when a message is received.
    #
    # The message itself can be obtained as a property on the event. For
    # the purpose of referring to this message in further actions, such as
    # explicitly accepting it) the delivery should be used. This is also
    # obtainable vi a property on the event.
    #
    # This method needs to be overridden.
    #
    # @param event [Qpid::Proton::Event::Event] The event.
    #
    def on_message(event)
    end

  end

end
