#
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
#

module Qpid

  module Proton

    # The +Messenger+ class defines a high level interface for
    # sending and receiving Messages. Every Messenger contains
    # a single logical queue of incoming messages and a single
    # logical queue of outgoing messages. These messages in these
    # queues may be destined for, or originate from, a variety of
    # addresses.
    #
    # The messenger interface is single-threaded.  All methods
    # except one ( #interrupt ) are intended to be used from within
    # the messenger thread.
    #
    # === Sending & Receiving Messages
    #
    # The Messenger class works in conjuction with the Message class. The
    # Message class is a mutable holder of message content.
    #
    # The put method copies its Message to the outgoing queue, and may
    # send queued messages if it can do so without blocking.  The send
    # method blocks until it has sent the requested number of messages,
    # or until a timeout interrupts the attempt.
    #
    # Similarly, the recv method receives messages into the incoming
    # queue, and may block as it attempts to receive the requested number
    # of messages,  or until timeout is reached. It may receive fewer
    # than the requested number.  The get method pops the
    # eldest Message off the incoming queue and copies it into the Message
    # object that you supply.  It will not block.
    #
    # The blocking attribute allows you to turn off blocking behavior entirely,
    # in which case send and recv will do whatever they can without
    # blocking, and then return.  You can then look at the number
    # of incoming and outgoing messages to see how much outstanding work
    # still remains.
    #
    class Messenger

      include Qpid::Proton::ExceptionHandling

      # Creates a new +Messenger+.
      #
      # The +name+ parameter is optional. If one is not provided then
      # a unique name is generated.
      #
      # ==== Options
      #
      # * name - the name (def. nil)
      #
      def initialize(name = nil)
        @impl = Cproton.pn_messenger(name)
        @selectables = {}
        ObjectSpace.define_finalizer(self, self.class.finalize!(@impl))
      end

      def self.finalize!(impl) # :nodoc:
        proc {
          Cproton.pn_messenger_free(impl)
        }
      end

      # Returns the name.
      #
      def name
        Cproton.pn_messenger_name(@impl)
      end

      # This property contains the password for the Messenger.private_key
      # file, or +nil+ if the file is not encrypted.
      #
      # ==== Arguments
      #
      # * password - the password
      #
      def password=(password)
        check_for_error(Cproton.pn_messenger_set_password(@impl, password))
      end

      # Returns the password property for the Messenger.private_key file.
      #
      def password
        Cproton.pn_messenger_get_password(@impl)
      end

      # Sets the timeout period, in milliseconds.
      #
      # A negative timeout period implies an infinite timeout.
      #
      # ==== Options
      #
      # * timeout - the timeout period
      #
      def timeout=(timeout)
        raise TypeError.new("invalid timeout: #{timeout}") if timeout.nil?
        Cproton.pn_messenger_set_timeout(@impl, timeout)
      end

      # Returns the timeout period
      #
      def timeout
        Cproton.pn_messenger_get_timeout(@impl)
      end

      # Returns true if blocking mode is enabled.
      #
      # Enable or disable blocking behavior during message sending
      # and receiving.  This affects every blocking call, with the
      # exception of work().  Currently, the affected calls are
      # send, recv, and stop.
      def blocking?
        Cproton.pn_messenger_is_blocking(@impl)
      end

      # Sets the blocking mode.
      def blocking=(blocking)
        Cproton.pn_messenger_set_blocking(@impl, blocking)
      end

      # Returns true if passive mode is enabled.
      #
      def passive?
        Cproton.pn_messenger_is_passive(@impl)
      end

      # Turns passive mode on or off.
      #
      # When set to passive mode, Messenger will not attempt to perform I/O
      # operations internally. In this mode it is necesssary to use the
      # Selectable type to drive any I/O needed to perform requestioned
      # actions.
      #
      # In this mode Messenger will never block.
      #
      def passive=(mode)
        Cproton.pn_messenger_set_passive(@impl, mode)
      end

      def deadline
        tstamp = Cproton.pn_messenger_deadline(@impl)
        return tstamp / 1000.0 unless tstamp.nil?
      end

      # Reports whether an error occurred.
      #
      def error?
        !Cproton.pn_messenger_errno(@impl).zero?
      end

      # Returns the most recent error number.
      #
      def errno
        Cproton.pn_messenger_errno(@impl)
      end

      # Returns the most recent error message.
      #
      def error
        Cproton.pn_error_text(Cproton.pn_messenger_error(@impl))
      end

      # Currently a no-op placeholder.
      # For future compatibility, do not send or recv messages
      # before starting the +Messenger+.
      #
      def start
        check_for_error(Cproton.pn_messenger_start(@impl))
      end

      # Stops the +Messenger+, preventing it from sending or receiving
      # any more messages.
      #
      def stop
        check_for_error(Cproton.pn_messenger_stop(@impl))
      end

      # Returns true iff a Messenger is in the stopped state.
      # This function does not block.
      #
      def stopped
        Cproton.pn_messenger_stopped(@impl)
      end

      # Subscribes the Messenger to messages originating from the
      # specified source. The source is an address as specified in the
      # Messenger introduction with the following addition. If the
      # domain portion of the address begins with the '~' character, the
      # Messenger will interpret the domain as host/port, bind to it,
      # and listen for incoming messages. For example "~0.0.0.0",
      # "amqp://~0.0.0.0" will all bind to any local interface and 
      # listen for incoming messages.  Ad address of # "amqps://~0.0.0.0" 
      # will only permit incoming SSL connections.
      #
      def subscribe(address)
        raise TypeError.new("invalid address: #{address}") if address.nil?
        subscription = Cproton.pn_messenger_subscribe(@impl, address)
        raise Qpid::Proton::ProtonError.new("Subscribe failed") if subscription.nil?
        Qpid::Proton::Subscription.new(subscription)
      end

      # Path to a certificate file for the +Messenger+.
      #
      # This certificate is used when the +Messenger+ accepts or establishes
      # SSL/TLS connections.  This property must be specified for the
      # Messenger to accept incoming SSL/TLS connections and to establish
      # client authenticated outgoing SSL/TLS connection.  Non client authenticated
      # outgoing SSL/TLS connections do not require this property.
      #
      # ==== Options
      #
      # * certificate - the certificate
      #
      def certificate=(certificate)
        Cproton.pn_messenger_set_certificate(@impl, certificate)
      end

      # Returns the path to a certificate file.
      #
      def certificate
        Cproton.pn_messenger_get_certificate(@impl)
      end

      # Path to a private key file for the +Messenger+.
      #
      # The property must be specified for the +Messenger+ to accept incoming
      # SSL/TLS connections and to establish client authenticated outgoing
      # SSL/TLS connections.  Non client authenticated SSL/TLS connections
      # do not require this property.
      #
      # ==== Options
      #
      # * key - the key file
      #
      def private_key=(key)
        Cproton.pn_messenger_set_private_key(@impl, key)
      end

      # Returns the path to a private key file.
      #
      def private_key
        Cproton.pn_messenger_get_private_key(@impl)
      end

      # A path to a database of trusted certificates for use in verifying the
      # peer on an SSL/TLS connection. If this property is +nil+, then the
      # peer will not be verified.
      #
      # ==== Options
      #
      # * certificates - the certificates path
      #
      def trusted_certificates=(certificates)
        Cproton.pn_messenger_set_trusted_certificates(@impl,certificates)
      end

      # The path to the databse of trusted certificates.
      #
      def trusted_certificates
        Cproton.pn_messenger_get_trusted_certificates(@impl)
      end

      # Places the content contained in the message onto the outgoing
      # queue of the Messenger.
      #
      # This method will never block, however it will send any unblocked 
      # Messages in the outgoing queue immediately and leave any blocked
      # Messages remaining in the outgoing queue.
      # The send call may then be used to block until the outgoing queue 
      # is empty.  The outgoing attribute may be used to check the depth
      # of the outgoing queue.
      #
      # ==== Options
      #
      # * message - the message
      #
      def put(message)
        raise TypeError.new("invalid message: #{message}") if message.nil?
        raise ArgumentError.new("invalid message type: #{message.class}") unless message.kind_of?(Message)
        # encode the message first
        message.pre_encode
        check_for_error(Cproton.pn_messenger_put(@impl, message.impl))
        return outgoing_tracker
      end

      # This call will block until the indicated number of messages
      # have been sent, or until the operation times out.
      # If n is -1 this call will block until all outgoing messages 
      # have been sent. If n is 0 then this call will send whatever 
      # it can without blocking.
      #
      def send(n = -1)
        check_for_error(Cproton.pn_messenger_send(@impl, n))
      end

      # Moves the message from the head of the incoming message queue into
      # the supplied message object. Any content in the supplied message
      # will be overwritten.
      # A tracker for the incoming Message is returned.  The tracker can
      # later be used to communicate your acceptance or rejection of the
      # Message.
      #
      # If no message is provided in the argument, then one is created. In
      # either case, the one returned will be the fetched message.
      #
      # ==== Options
      #
      # * msg - the (optional) +Message+ instance to be used
      #
      def get(msg = nil)
        msg_impl = nil
        if msg.nil? then
          msg_impl = nil
        else
          msg_impl = msg.impl
        end
        check_for_error(Cproton.pn_messenger_get(@impl, msg_impl))
        msg.post_decode unless msg.nil?
        return incoming_tracker
      end

      # Receives up to limit messages into the incoming queue.  If no value
      # for limit is supplied, this call will receive as many messages as it
      # can buffer internally.  If the Messenger is in blocking mode, this
      # call will block until at least one Message is available in the
      # incoming queue.
      #
      # Options ====
      #
      # * limit - the maximum number of messages to receive
      #
      def receive(limit = -1)
        check_for_error(Cproton.pn_messenger_recv(@impl, limit))
      end

      def receiving
        Cproton.pn_messenger_receiving(@impl)
      end

      # Attempts interrupting of the messenger thread.
      #
      # The Messenger interface is single-threaded, and this is the only
      # function intended to be called from outside of is thread.
      #
      # Call this from a non-Messenger thread to interrupt it while it
      # is blocking. This will cause a ::InterruptError to be raised.
      #
      # If there is no currently blocking call, then the next blocking
      # call will be affected, even if it is within the same thread that
      # originated the interrupt.
      #
      def interrupt
        check_for_error(Cproton.pn_messenger_interrupt(@impl))
      end

      # Sends or receives any outstanding messages queued for a Messenger.
      #
      # This will block for the indicated timeout.  This method may also do I/O
      # other than sending and receiving messages.  For example, closing
      # connections after stop() has been called.
      # 
      def work(timeout=-1)
        err = Cproton.pn_messenger_work(@impl, timeout)
        if (err == Cproton::PN_TIMEOUT) then
          return false
        else
          check_for_error(err)
          return true
        end
      end

      # Returns the number messages in the outgoing queue that have not been
      # transmitted.
      #
      def outgoing
        Cproton.pn_messenger_outgoing(@impl)
      end

      # Returns the number of messages in the incoming queue that have not
      # been retrieved.
      #
      def incoming
        Cproton.pn_messenger_incoming(@impl)
      end

      # Adds a routing rule to the Messenger's internal routing table.
      #
      # The route procedure may be used to influence how a Messenger will
      # internally treat a given address or class of addresses. Every call
      # to the route procedure will result in Messenger appending a routing
      # rule to its internal routing table.
      #
      # Whenever a Message is presented to a Messenger for delivery, it
      # will match the address of this message against the set of routing
      # rules in order. The first rule to match will be triggered, and
      # instead of routing based on the address presented in the message,
      # the Messenger will route based on the address supplied in the rule.
      #
      # The pattern matching syntax supports two types of matches, a '%'
      # will match any character except a '/', and a '*' will match any
      # character including a '/'.
      #
      # A routing address is specified as a normal AMQP address, however it
      # may additionally use substitution variables from the pattern match
      # that triggered the rule.
      #
      # ==== Arguments
      #
      # * pattern - the address pattern
      # * address - the target address
      #
      # ==== Examples
      #
      #   # route messages sent to foo to the destionaty amqp://foo.com
      #   messenger.route("foo", "amqp://foo.com")
      #
      #   # any message to foobar will be routed to amqp://foo.com/bar
      #   messenger.route("foobar", "amqp://foo.com/bar")
      #
      #   # any message to bar/<path> will be routed to the same path within
      #   # the amqp://bar.com domain
      #   messenger.route("bar/*", "amqp://bar.com/$1")
      #
      #   # route all Message objects over TLS
      #   messenger.route("amqp:*", "amqps:$1")
      #
      #   # supply credentials for foo
      #   messenger.route("amqp://foo.com/*", "amqp://user:password@foo.com/$1")
      #
      #   # supply credentials for all domains
      #   messenger.route("amqp://*", "amqp://user:password@$1")
      #
      #   # route all addresses through a single proxy while preserving the
      #   # original destination
      #   messenger.route("amqp://%$/*", "amqp://user:password@proxy/$1/$2")
      #
      #   # route any address through a single broker
      #   messenger.route("*", "amqp://user:password@broker/$1")
      #
      def route(pattern, address)
        check_for_error(Cproton.pn_messenger_route(@impl, pattern, address))
      end

      # Similar to #route, except that the destination of
      # the Message is determined before the message address is rewritten.
      #
      # The outgoing address is only rewritten after routing has been
      # finalized.  If a message has an outgoing address of
      # "amqp://0.0.0.0:5678", and a rewriting rule that changes its
      # outgoing address to "foo", it will still arrive at the peer that
      # is listening on "amqp://0.0.0.0:5678", but when it arrives there,
      # the receiver will see its outgoing address as "foo".
      #
      # The default rewrite rule removes username and password from addresses
      # before they are transmitted.
      #
      # ==== Arguments
      #
      # * pattern - the outgoing address
      # * address - the target address
      #
      def rewrite(pattern, address)
        check_for_error(Cproton.pn_messenger_rewrite(@impl, pattern, address))
      end

      def selectable
        impl = Cproton.pn_messenger_selectable(@impl)

        # if we don't have any selectables, then return
        return nil if impl.nil?

        fd = Cproton.pn_selectable_fd(impl)

        selectable = @selectables[fd]
        if selectable.nil?
          selectable = Selectable.new(self, impl)
          @selectables[fd] = selectable
        end
        return selectable
      end

      # Returns a +Tracker+ for the message most recently sent via the put
      # method.
      #
      def outgoing_tracker
        impl = Cproton.pn_messenger_outgoing_tracker(@impl)
        return nil if impl == -1
        Qpid::Proton::Tracker.new(impl)
      end

      # Returns a +Tracker+ for the most recently received message.
      #
      def incoming_tracker
        impl = Cproton.pn_messenger_incoming_tracker(@impl)
        return nil if impl == -1
        Qpid::Proton::Tracker.new(impl)
      end

      # Signal the sender that you have acted on the Message
      # pointed to by the tracker.  If no tracker is supplied,
      # then all messages that have been returned by the get
      # method are accepted, except those that have already been
      # auto-settled by passing beyond your incoming window size.
      #
      # ==== Options
      #
      # * tracker - the tracker
      #
      def accept(tracker = nil)
        raise TypeError.new("invalid tracker: #{tracker}") unless tracker.nil? or valid_tracker?(tracker)
        if tracker.nil? then
          tracker = self.incoming_tracker
          flag = Cproton::PN_CUMULATIVE
        else
          flag = 0
        end
        check_for_error(Cproton.pn_messenger_accept(@impl, tracker.impl, flag))
      end

      # Rejects the incoming message identified by the tracker.
      # If no tracker is supplied, all messages that have been returned
      # by the get method are rejected, except those that have already
      # been auto-settled by passing beyond your outgoing window size.
      #
      # ==== Options
      #
      # * tracker - the tracker
      #
      def reject(tracker)
        raise TypeError.new("invalid tracker: #{tracker}") unless tracker.nil? or valid_tracker?(tracker)
        if tracker.nil? then
          tracker = self.incoming_tracker
          flag = Cproton::PN_CUMULATIVE
        else
          flag = 0
        end
        check_for_error(Cproton.pn_messenger_reject(@impl, tracker.impl, flag))
      end

      # Gets the last known remote state of the delivery associated with
      # the given tracker, as long as the Message is still within your 
      # outgoing window. (Also works on incoming messages that are still 
      # within your incoming queue. See TrackerStatus for details on the 
      # values returned.
      #
      # ==== Options
      #
      # * tracker - the tracker
      #
      def status(tracker)
        raise TypeError.new("invalid tracker: #{tracker}") unless valid_tracker?(tracker)
        Qpid::Proton::TrackerStatus.by_value(Cproton.pn_messenger_status(@impl, tracker.impl))
      end

      # Frees a Messenger from tracking the status associated
      # with a given tracker. If you don't supply a tracker, all
      # outgoing messages up to the most recent will be settled.
      #
      # ==== Options
      #
      # * tracker - the tracker
      #
      # ==== Examples
      #
      def settle(tracker)
        raise TypeError.new("invalid tracker: #{tracker}") unless valid_tracker?(tracker)
        if tracker.nil? then
          tracker = self.incoming_tracker
          flag = Cproton::PN_CUMULATIVE
        else
          flag = 0
        end
        Cproton.pn_messenger_settle(@impl, tracker.impl, flag)
      end

      # Sets the incoming window.
      #
      # The Messenger will track the remote status of this many incoming 
      # deliveries after they have been accepted or rejected.
      #
      # Messages enter this window only when you take them into your application
      # using get().  If your incoming window size is n, and you get n+1 messages
      # without explicitly accepting or rejecting the oldest message, then the
      # message that passes beyond the edge of the incoming window will be 
      # assigned the default disposition of its link.
      #
      # ==== Options
      #
      # * window - the window size
      #
      def incoming_window=(window)
        raise TypeError.new("invalid window: #{window}") unless valid_window?(window)
        check_for_error(Cproton.pn_messenger_set_incoming_window(@impl, window))
      end

      # Returns the incoming window.
      #
      def incoming_window
        Cproton.pn_messenger_get_incoming_window(@impl)
      end

      # Sets the outgoing window.
      #
      # The Messenger will track the remote status of this many outgoing 
      # deliveries after calling send.
      # A Message enters this window when you call the put() method with the
      # message.  If your outgoing window size is n, and you call put n+1
      # times, status information will no longer be available for the
      # first message.
      #
      # ==== Options
      #
      # * window - the window size
      #
      def outgoing_window=(window)
        raise TypeError.new("invalid window: #{window}") unless valid_window?(window)
        check_for_error(Cproton.pn_messenger_set_outgoing_window(@impl, window))
      end

      # Returns the outgoing window.
      #
      def outgoing_window
        Cproton.pn_messenger_get_outgoing_window(@impl)
      end

      # Unregisters a selectable object.
      def unregister_selectable(fileno) # :nodoc:
        @selectables.delete(fileno)
      end

      private

      def valid_tracker?(tracker)
        !tracker.nil? && tracker.is_a?(Qpid::Proton::Tracker)
      end

      def valid_window?(window)
        !window.nil? && [Float, Fixnum].include?(window.class)
      end

    end

  end

end
