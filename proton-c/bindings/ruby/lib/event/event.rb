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

module Qpid::Proton

  module Event

    # @private
    def self.event_type(const_name, method_name = nil) # :nodoc:
      unless Cproton.const_defined?(const_name)
        raise RuntimeError.new("no such constant: #{const_name}")
      end

      const_value = Cproton.const_get(const_name)
      method_name = "on_#{const_name.to_s[3..-1]}".downcase if method_name.nil?

      EventType.new(const_value, method_name)
    end

    # Defined as a programming convenience. No even of this type will ever
    # be generated.
    NONE = event_type(:PN_EVENT_NONE)

    # A reactor has been started.
    REACTOR_INIT = event_type(:PN_REACTOR_INIT)
    # A reactor has no more events to process.
    REACTOR_QUIESCED = event_type(:PN_REACTOR_QUIESCED)
    # A reactor has been stopred.
    REACTOR_FINAL = event_type(:PN_REACTOR_FINAL)

    # A timer event has occurred.
    TIMER_TASK = event_type(:PN_TIMER_TASK)

    # A connection has been created. This is the first even that will ever
    # be issued for a connection.
    CONNECTION_INIT = event_type(:PN_CONNECTION_INIT)
    # A conneciton has been bound toa  transport.
    CONNECTION_BOUND = event_type(:PN_CONNECTION_BOUND)
    # A connection has been unbound from its transport.
    CONNECTION_UNBOUND = event_type(:PN_CONNECTION_UNBOUND)
    # A local connection endpoint has been opened.
    CONNECTION_LOCAL_OPEN = event_type(:PN_CONNECTION_LOCAL_OPEN)
    # A local connection endpoint has been closed.
    CONNECTION_LOCAL_CLOSE = event_type(:PN_CONNECTION_LOCAL_CLOSE)
    # A remote endpoint has opened its connection.
    CONNECTION_REMOTE_OPEN = event_type(:PN_CONNECTION_REMOTE_OPEN)
    # A remote endpoint has closed its connection.
    CONNECTION_REMOTE_CLOSE = event_type(:PN_CONNECTION_REMOTE_CLOSE)
    # A connection has been freed and any outstanding processing has been
    # completed. This is the final event htat will ever be issued for a
    # connection
    CONNECTION_FINAL = event_type(:PN_CONNECTION_FINAL)

    # A session has been created. This is the first event that will ever be
    # issues for a session.
    SESSION_INIT = event_type(:PN_SESSION_INIT)
    # A local session endpoint has been opened.
    SESSION_LOCAL_OPEN = event_type(:PN_SESSION_LOCAL_OPEN)
    # A local session endpoint has been closed.
    SESSION_LOCAL_CLOSE = event_type(:PN_SESSION_LOCAL_CLOSE)
    # A remote endpoint has opened its session.
    SESSION_REMOTE_OPEN = event_type(:PN_SESSION_REMOTE_OPEN)
    # A remote endpoint has closed its session.
    SESSION_REMOTE_CLOSE = event_type(:PN_SESSION_REMOTE_CLOSE)
    # A session has been freed and any outstanding processing has been
    # completed. This is the final event that will ever be issued for a
    # session
    SESSION_FINAL = event_type(:PN_SESSION_FINAL)

    # A link has been created. This is the first event that will ever be
    # issued for a link.
    LINK_INIT = event_type(:PN_LINK_INIT)
    # A local link endpoint has been opened.
    LINK_LOCAL_OPEN = event_type(:PN_LINK_LOCAL_OPEN)
    # A local link endpoint has been closed.
    LINK_LOCAL_CLOSE = event_type(:PN_LINK_LOCAL_CLOSE)
    # A local link endpoint has been detached.
    LINK_LOCAL_DETACH = event_type(:PN_LINK_LOCAL_DETACH)
    # A remote endpoint has opened its link.
    LINK_REMOTE_OPEN = event_type(:PN_LINK_REMOTE_OPEN)
    # A remote endpoint has closed its link.
    LINK_REMOTE_CLOSE = event_type(:PN_LINK_REMOTE_CLOSE)
    # A remote endpoint has detached its link.
    LINK_REMOTE_DETACH = event_type(:PN_LINK_REMOTE_DETACH)
    # The flow control state for a link has changed.
    LINK_FLOW = event_type(:PN_LINK_FLOW)
    # A link has been freed and any outstanding processing has been completed.
    # This is the final event htat will ever be issued for a link.
    LINK_FINAL = event_type(:PN_LINK_FINAL)

    # A delivery has been created or updated.
    DELIVERY = event_type(:PN_DELIVERY)

    # A transport has new data to read and/or write.
    TRANSPORT = event_type(:PN_TRANSPORT)
    # Indicates that a transport error has occurred.
    # @see Transport#condition To access the details of the error.
    TRANSPORT_ERROR = event_type(:PN_TRANSPORT_ERROR)
    # Indicates that the head of a transport has been closed. This means the
    # transport will never produce more bytes for output to the network.
    TRANSPORT_HEAD_CLOSED = event_type(:PN_TRANSPORT_HEAD_CLOSED)
    # Indicates that the trail of a transport has been closed. This means the
    # transport will never be able to process more bytes from the network.
    TRANSPORT_TAIL_CLOSED = event_type(:PN_TRANSPORT_TAIL_CLOSED)
    # Indicates that both the head and tail of a transport are closed.
    TRANSPORT_CLOSED = event_type(:PN_TRANSPORT_CLOSED)

    SELECTABLE_INIT = event_type(:PN_SELECTABLE_INIT)
    SELECTABLE_UPDATED = event_type(:PN_SELECTABLE_UPDATED)
    SELECTABLE_READABLE = event_type(:PN_SELECTABLE_READABLE)
    SELECTABLE_WRITABLE = event_type(:PN_SELECTABLE_WRITABLE)
    SELECTABLE_EXPIRED = event_type(:PN_SELECTABLE_EXPIRED)
    SELECTABLE_ERROR = event_type(:PN_SELECTABLE_ERROR)
    SELECTABLE_FINAL = event_type(:PN_SELECTABLE_FINAL)

    # An Event provides notification of a state change within the protocol
    # engine.
    #
    # Every event has a type that identifies what sort of state change has
    # occurred, along with a pointer to the object whose state has changed,
    # and also any associated objects.
    #
    # For more details on working with Event, please refer to Collector.
    #
    # @see Qpid::Proton::Event The list of predefined events.
    #
    class Event < EventBase

      # @private
      include Qpid::Proton::Util::ClassWrapper
      # @private
      include Qpid::Proton::Util::Wrapper

      # Creates a Ruby object for the given pn_event_t.
      #
      # @private
      def self.wrap(impl, number = nil)
        return nil if impl.nil?

        result = self.fetch_instance(impl, :pn_event_attachments)
        return result unless result.nil?
        number = Cproton.pn_event_type(impl) if number.nil?
        event = Event.new(impl, number)
        return event.context if event.context.is_a? EventBase
        return event
      end

      # @private
      def initialize(impl, number)
        @impl = impl
        class_name = Cproton.pn_class_name(Cproton.pn_event_class(impl))
        context = class_wrapper(class_name, Cproton.pn_event_context(impl))
        event_type = EventType.by_type(Cproton.pn_event_type(impl))
        super(class_name, context, event_type)
        @type = EventType.by_type(number)
        self.class.store_instance(self, :pn_event_attachments)
      end

      # Notifies the handler(s) of this event.
      #
      # If a handler responds to the event's method then that method is invoked
      # and passed the event. Otherwise, if the handler defines the
      # +on_unhandled+ method, then that will be invoked instead.
      #
      # If the handler defines a +handlers+ method then that will be invoked and
      # passed the event afterward.
      #
      # @example
      #
      #   class FallbackEventHandler
      #
      #     # since it now defines a handlers method, any event will iterate
      #     # through them and invoke the +dispatch+ method on each
      #     attr_accessor handlers
      #
      #     def initialize
      #       @handlers = []
      #     end
      #
      #     # invoked for any event not otherwise handled
      #     def on_unhandled(event)
      #       puts "Unable to invoke #{event.type.method} on #{event.context}."
      #     end
      #
      #   end
      #
      # @param handler [Object] An object which implements either the event's
      #    handler method or else responds to :handlers with an array of other
      #    handlers.
      #
      def dispatch(handler, type = nil)
        type = @type if type.nil?
        if handler.is_a?(Qpid::Proton::Handler::WrappedHandler)
          Cproton.pn_handler_dispatch(handler.impl, @impl, type.number)
        else
          result = Qpid::Proton::Event.dispatch(handler, type.method, self)
          if (result != "DELEGATED") && handler.respond_to?(:handlers)
            handler.handlers.each do |hndlr|
              self.dispatch(hndlr)
            end
          end
        end
      end

      # Returns the reactor for this event.
      #
      # @return [Reactor, nil] The reactor.
      #
      def reactor
        impl = Cproton.pn_event_reactor(@impl)
        Qpid::Proton::Util::ClassWrapper::WRAPPERS["pn_reactor"].call(impl)
      end

      def container
        impl = Cproton.pn_event_reactor(@impl)
        Qpid::Proton::Util::ClassWrapper::WRAPPERS["pn_reactor"].call(impl)
      end

      # Returns the transport for this event.
      #
      # @return [Transport, nil] The transport.
      #
      def transport
        Qpid::Proton::Transport.wrap(Cproton.pn_event_transport(@impl))
      end

      # Returns the Connection for this event.
      #
      # @return [Connection, nil] The connection.
      #
      def connection
        Qpid::Proton::Connection.wrap(Cproton.pn_event_connection(@impl))
      end

      # Returns the Session for this event.
      #
      # @return [Session, nil] The session
      #
      def session
        Qpid::Proton::Session.wrap(Cproton.pn_event_session(@impl))
      end

      # Returns the Link for this event.
      #
      # @return [Link, nil] The link.
      #
      def link
        Qpid::Proton::Link.wrap(Cproton.pn_event_link(@impl))
      end

      # Returns the Sender, or nil if there is no Link, associated  with this
      # event if that link is a sender.
      #
      # @return [Sender, nil] The sender.
      #
      def sender
        return self.link if !self.link.nil? && self.link.sender?
      end

      # Returns the Receiver, or nil if there is no Link, associated with this
      # event if that link is a receiver.
      #
      # @return [Receiver, nil] The receiver.
      #
      def receiver
        return self.link if !self.link.nil? && self.link.receiver?
      end

      # Returns the Delivery associated with this event.
      #
      # @return [Delivery, nil] The delivery.
      #
      def delivery
        Qpid::Proton::Delivery.wrap(Cproton.pn_event_delivery(@impl))
      end

      # Sets the message.
      #
      # @param message [Qpid::Proton::Message] The message
      #
      def message=(message)
        @message = message
      end

      # Returns the message.
      #
      # @return [Qpid::Proton::Message] The message.
      #
      def message
        @message
      end

      # @private
      def to_s
        "#{self.type}(#{self.context})"
      end

    end

  end

end
