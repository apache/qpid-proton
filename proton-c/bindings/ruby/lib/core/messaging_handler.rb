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

  # A handler for AMQP messaging events.
  #
  # Subclass the handler and provide the #on_xxx methods with your event-handling code.
  #
  class MessagingHandler

    # Create a {MessagingHandler}
    # @option opts [Integer] :prefetch (10)
    #  The number of messages to  fetch in advance, 0 disables prefetch.
    # @option opts [Boolean] :auto_accept  (true)
    #  If true, incoming messages are accepted automatically after {#on_message}.
    #  If false, the application can accept, reject or release the message
    #  by calling methods on {Delivery} when the message has been processed.
    # @option opts [Boolean] :auto_settle (true) If true, outgoing
    #  messages are settled automatically when the remote peer settles. If false,
    #  the application must call {Delivery#settle} explicitly.
    # @option opts [Boolean] :auto_open (true)
    #  If true, incoming connections are  opened automatically.
    #  If false, the application must call {Connection#open} to open incoming connections.
    # @option opts [Boolean] :auto_close (true)
    #  If true, respond to a remote close automatically with a local close.
    #  If false, the application must call {Connection#close} to finish closing connections.
    # @option opts [Boolean] :peer_close_is_error (false)
    #  If true, and the remote peer closes the connection without an error condition,
    #  the set the local error condition {Condition}("error", "unexpected peer close")
    def initialize(opts=nil)
      @options = opts && opts.clone
    end

    # @return [Hash] handler options, see {#initialize}
    attr_reader :options

    # @!group Most common events

    # @!method on_container_start(container)
    # The container event loop is started
    # @param container [Container] The container.

    # @!method on_container_stop(container)
    # The container event loop is stopped
    # @param container [Container] The container.

    # @!method on_message(delivery, message)
    # A message is received.
    # @param delivery [Delivery] The delivery.
    # @param message [Message] The message

    # @!method on_sendable(sender)
    # A message can be sent
    # @param sender [Sender] The sender.

    # @!endgroup

    # @!group Endpoint lifecycle events

    # @!method on_connection_open(connection)
    # The remote peer opened the connection
    # @param connection

    # @!method on_connection_close(connection)
    # The remote peer closed the connection
    # @param connection

    # @!method on_connection_error(connection)
    # The remote peer closed the connection with an error condition
    # @param connection

    # @!method on_session_open(session)
    # The remote peer opened the session
    # @param session

    # @!method on_session_close(session)
    # The remote peer closed the session
    # @param session

    # @!method on_session_error(session)
    # The remote peer closed the session with an error condition
    # @param session

    # @!method on_sender_open(sender)
    # The remote peer opened the sender
    # @param sender

    # @!method on_sender_detach(sender)
    # The remote peer detached the sender
    # @param sender

    # @!method on_sender_close(sender)
    # The remote peer closed the sender
    # @param sender

    # @!method on_sender_error(sender)
    # The remote peer closed the sender with an error condition
    # @param sender

    # @!method on_receiver_open(receiver)
    # The remote peer opened the receiver
    # @param receiver

    # @!method on_receiver_detach(receiver)
    # The remote peer detached the receiver
    # @param receiver

    # @!method on_receiver_close(receiver)
    # The remote peer closed the receiver
    # @param receiver

    # @!method on_receiver_error(receiver)
    # The remote peer closed the receiver with an error condition
    # @param receiver

    # @!endgroup

    # @!group Delivery events

    # @!method on_tracker_accept(tracker)
    # The receiving end accepted a delivery
    # @param tracker [Tracker] The tracker.

    # @!method on_tracker_reject(tracker)
    # The receiving end rejected a delivery
    # @param tracker [Tracker] The tracker.

    # @!method on_tracker_release(tracker)
    # The receiving end released a delivery
    # @param tracker [Tracker] The tracker.

    # @!method on_tracker_modify(tracker)
    # The receiving end modified a delivery
    # @param tracker [Tracker] The tracker.

    # @!method on_tracker_settle(tracker)
    # The receiving end settled a delivery
    # @param tracker [Tracker] The tracker.

    # @!method on_delivery_settle(delivery)
    # The sending end settled a delivery
    # @param delivery [Delivery] The delivery.

    # @!endgroup

    # @!group Flow control events

    # @!method on_sender_drain_start(sender)
    # The remote end of the sender requested draining
    # @param sender [Sender] The sender.

    # @!method on_receiver_drain_finish(receiver)
    # The remote end of the receiver completed draining
    # @param receiver [Receiver] The receiver.

    # @!endgroup

    # @!group Transport events

    # @!method on_transport_open(transport)
    # The underlying network channel opened
    # @param transport [Transport] The transport.

    # @!method on_transport_close(transport)
    # The underlying network channel closed
    # @param transport [Transport] The transport.

    # @!method on_transport_error(transport)
    # The underlying network channel is closing due to an error.
    # @param transport [Transport] The transport.

    # @!endgroup

    # @!group Unhandled events

    # @!method on_error(error_condition)
    # The fallback error handler when no specific on_xxx_error is defined
    # @param error_condition [Condition] Provides information about the error.

    # @!method on_unhandled(method_name, *args)
    # Called for events with no handler. Similar to ruby's standard #method_
    # @param method_name [Symbol] Name of the event method that would have been called.
    # @param args [Array] Arguments that would have been passed

    # @!endgroup
  end
end
