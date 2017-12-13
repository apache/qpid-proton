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


module Qpid::Proton
  module Handler

    # @deprecated use {Qpid::Proton::MessagingHandler}
    class MessagingHandler
      include Util::Deprecation

      # @private
      def proton_adapter_class() Handler::ReactorMessagingAdapter; end


      # @overload initialize(opts)
      #   Create a {MessagingHandler} with options +opts+
      #   @option opts [Integer] :prefetch (10)
      #    The number of messages to  fetch in advance, 0 disables prefetch.
      #   @option opts [Boolean] :auto_accept  (true)
      #    If true, incoming messages are accepted automatically after {#on_message}.
      #    If false, the application can accept, reject or release the message
      #    by calling methods on {Delivery} when the message has been processed.
      #   @option opts [Boolean] :auto_settle (true) If true, outgoing
      #    messages are settled automatically when the remote peer settles. If false,
      #    the application must call {Delivery#settle} explicitly.
      #   @option opts [Boolean] :auto_open (true)
      #    If true, incoming connections are  opened automatically.
      #    If false, the application must call {Connection#open} to open incoming connections.
      #   @option opts [Boolean] :auto_close (true)
      #    If true, respond to a remote close automatically with a local close.
      #    If false, the application must call {Connection#close} to finish closing connections.
      #   @option opts [Boolean] :peer_close_is_error (false)
      #    If true, and the remote peer closes the connection without an error condition,
      #    the set the local error condition {Condition}("error", "unexpected peer close")
      #
      # @overload initialize(prefetch=10, auto_accept=true, auto_settle=true, peer_close_is_error=false)
      # @deprecated use +initialize(opts)+ overload
      def initialize(*args)
        deprecated MessagingHandler, Qpid::Proton::MessagingHandler
        @options = {}
        if args.size == 1 && args[0].is_a?(Hash)
          @options.replace(args[0])
        else                      # Fill options from deprecated fixed arguments
          [:prefetch, :auto_accept, :auto_settle, :peer_close_is_error].each do |k|
            opts[k] = args.shift unless args.empty?
          end
        end
        # NOTE: the options are processed by {Handler::Adapater}
      end

      public

      # @return [Hash] handler options, see {#initialize}
      attr_reader :options

      # @!method on_transport_error(event)
      # Called when the transport fails or closes unexpectedly.
      # @param event [Event] The event.

      # !@method on_connection_error(event)
      # Called when the peer closes the connection with an error condition.
      # @param event [Event] The event.

      # @!method on_session_error(event)
      # Called when the peer closes the session with an error condition.
      # @param event [Qpid:Proton::Event] The event.

      # @!method on_link_error(event)
      # Called when the peer closes the link with an error condition.
      # @param event [Event] The event.

      # @!method on_start(event)
      # Called when the event loop starts.
      # @param event [Event] The event.

      # @!method on_connection_closed(event)
      # Called when the connection is closed.
      # @param event [Event] The event.

      # @!method on_session_closed(event)
      # Called when the session is closed.
      # @param event [Event] The event.

      # @!method on_link_closed(event)
      # Called when the link is closed.
      # @param event [Event] The event.

      # @!method on_connection_closing(event)
      # Called when the peer initiates the closing of the connection.
      # @param event [Event] The event.

      # @!method on_session_closing(event)
      # Called when the peer initiates the closing of the session.
      # @param event [Event] The event.

      # @!method on_link_closing(event)
      # Called when the peer initiates the closing of the link.
      # @param event [Event] The event.

      # @!method on_disconnected(event)
      # Called when the socket is disconnected.
      # @param event [Event] The event.

      # @!method on_sendable(event)
      # Called when the sender link has credit and messages can therefore
      # be transferred.
      # @param event [Event] The event.

      # @!method on_accepted(event)
      # Called when the remote peer accepts an outgoing message.
      # @param event [Event] The event.

      # @!method on_rejected(event)
      # Called when the remote peer rejects an outgoing message.
      # @param event [Event] The event.

      # @!method on_released(event)
      # Called when the remote peer releases an outgoing message for re-delivery as-is.
      # @param event [Event] The event.

      # @!method on_modified(event)
      # Called when the remote peer releases an outgoing message for re-delivery with modifications.
      # @param event [Event] The event.

      # @!method on_settled(event)
      # Called when the remote peer has settled hte outgoing message.
      # This is the point at which it should never be retransmitted.
      # @param event [Event] The event.

      # @!method on_message(event)
      # Called when a message is received.
      #
      # The message is available from {Event#message}, to accept or reject the message
      # use {Event#delivery}
      # @param event [Event] The event.

      # @!method on_aborted(event)
      # Called when message delivery is aborted by the sender.
      # The {Event#delivery} provides information about the delivery, but the message should be ignored.

      # @!method on_error(event)
      # If +on_xxx_error+ method is missing, {#on_error} is called instead.
      # If {#on_error} is missing, the connection is closed with the error.
      # @param event [Event] the event, {Event#method} provides the original method name.

      # @!method on_unhandled(event)
      # If an +on_xxx+ method is missing, {#on_unhandled} is called instead.
      # @param event [Event] the event, {Event#method} provides the original method name.
    end
  end
end
