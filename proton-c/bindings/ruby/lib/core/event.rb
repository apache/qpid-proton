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

  # @deprecated Only used with the deprecated {Handler::MessagingHandler} API.
  class Event
    private
    include Util::Deprecation

    PROTON_METHOD_PREFIX = "pn_disposition"
    include Util::Wrapper

    EVENT_TYPE_NAMES = [:PN_EVENT_NONE,
                        :PN_CONNECTION_INIT,
                        :PN_CONNECTION_BOUND,
                        :PN_CONNECTION_UNBOUND,
                        :PN_CONNECTION_LOCAL_OPEN,
                        :PN_CONNECTION_REMOTE_OPEN,
                        :PN_CONNECTION_LOCAL_CLOSE,
                        :PN_CONNECTION_REMOTE_CLOSE,
                        :PN_CONNECTION_FINAL,
                        :PN_SESSION_INIT,
                        :PN_SESSION_LOCAL_OPEN,
                        :PN_SESSION_REMOTE_OPEN,
                        :PN_SESSION_LOCAL_CLOSE,
                        :PN_SESSION_REMOTE_CLOSE,
                        :PN_SESSION_FINAL,
                        :PN_LINK_INIT,
                        :PN_LINK_LOCAL_OPEN,
                        :PN_LINK_REMOTE_OPEN,
                        :PN_LINK_LOCAL_CLOSE,
                        :PN_LINK_REMOTE_CLOSE,
                        :PN_LINK_LOCAL_DETACH,
                        :PN_LINK_REMOTE_DETACH,
                        :PN_LINK_FLOW,
                        :PN_LINK_FINAL,
                        :PN_DELIVERY,
                        :PN_TRANSPORT,
                        :PN_TRANSPORT_AUTHENTICATED,
                        :PN_TRANSPORT_ERROR,
                        :PN_TRANSPORT_HEAD_CLOSED,
                        :PN_TRANSPORT_TAIL_CLOSED,
                        :PN_TRANSPORT_CLOSED]

    TYPE_METHODS = EVENT_TYPE_NAMES.each_with_object({}) do |n, h|
      type = Cproton.const_get(n)
      h[type] = "on_#{Cproton.pn_event_type_name(type)[3..-1]}".downcase.to_sym
    end

    # Use Event.new(impl) to wrap a C event, or Event.new(nil, method, context)
    # to create a pure-ruby event.
    def initialize(impl, method=nil, context=nil)
      @impl, @method, @context = impl, method, context
      @method ||= TYPE_METHODS[Cproton.pn_event_type(@impl)] if @impl
    end

    # Get the context if it is_a?(clazz), else call method on the context
    def get(clazz, method=nil)
      (ctx = context).is_a?(clazz) ? ctx : ctx.__send__(method) rescue nil
    end

    def _context
      x = Cproton.pn_event_context(@impl)
      case Cproton.pn_class_id(Cproton.pn_event_class(@impl))
      when Cproton::CID_pn_transport then Transport.wrap(Cproton.pn_cast_pn_transport(x))
      when Cproton::CID_pn_connection then Connection.wrap(Cproton.pn_cast_pn_connection(x))
      when Cproton::CID_pn_session then Session.wrap(Cproton.pn_cast_pn_session(x))
      when Cproton::CID_pn_link then Link.wrap(Cproton.pn_cast_pn_link(x))
      when Cproton::CID_pn_delivery then Delivery.wrap(Cproton.pn_cast_pn_delivery(x))
      end
    end

    public

    # Call handler.{#method}(self) if handler.respond_to? {#method}
    # @return [Boolean] true if handler responded to the method, nil if not.
    def dispatch(handler)
      (handler.__send__(@method, self); true) if handler.respond_to? @method
    end

    # @return [Symbol] method name that this event will call in {#dispatch}
    attr_accessor :method

    alias type method

    # @return [Object] the event context object
    def context; return @context ||= _context; end

    # @return [Container, nil] container for this event
    def container() @container ||= get(Container, :container); end

    # @return [Transport, nil] transport for this event
    def transport() @transport ||= get(Transport, :transport); end

    # @return [Connection, nil] the connection for this event
    def connection() @connection ||= get(Connection, :connection); end

    # @return [Session, nil] session for this event
    def session() @session ||= get(Session, :session); end

    # @return [Link, nil] link for this event
    def link() @link ||= get(Link, :link); end

    # @return [Sender, nil] sender associated with this event
    def sender() link if link && link.sender?; end

    # @return [Receiver, nil] receiver associated with this event
    def receiver() link if link && link.receiver?; end

    # @return [Delivery, nil] delivery for this event
    def delivery()
      @delivery ||= case context
                    when Delivery then @delivery = @context
                      # deprecated: for backwards compat allow a Tracker to be treated as a Delivery
                    when Tracker then @delivery = Delivery.new(context.impl)
                    end
    end

    # @return [Tracker, nil] delivery for this event
    def tracker() @tracker ||= get(Tracker); end

    # @return [Message, nil] message for this event
    def message() @message ||= delivery.message if delivery; end

    def to_s() "#{self.class}(#{method}, #{context})"; end
    def inspect() "#{self.class}(#{method.inspect}, #{context.inspect})"; end

    # @return [Condition] Error condition associated with this event or nil if none.
    def condition
      (context.remote_condition if context.respond_to? :remote_condition) ||
        (context.condition if context.respond_to? :condition)
    end

    # @deprecated use {#container}
    deprecated_alias :reactor, :container

    # @private
    Event = self
  end
end
