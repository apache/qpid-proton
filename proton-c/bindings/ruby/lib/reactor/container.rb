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

module Qpid::Proton::Reactor

  # @private
  class InternalTransactionHandler < Qpid::Proton::Handler::OutgoingMessageHandler

    def initialize
      super
    end

    def on_settled(event)
      if event.delivery.respond_to? :transaction
        event.transaction = event.delivery.transaction
        event.delivery.transaction.handle_outcome(event)
      end
    end

  end


  # A representation of the AMQP concept of a container which, loosely
  # speaking, is something that establishes links to or from another
  # container on which messages are transferred.
  #
  # This is an extension to the Reactor classthat adds convenience methods
  # for creating instances of Qpid::Proton::Connection, Qpid::Proton::Sender
  # and Qpid::Proton::Receiver.
  #
  # @example
  #
  class Container < Reactor

    include Qpid::Proton::Util::Reactor

    include Qpid::Proton::Util::UUID

    attr_accessor :container_id
    attr_accessor :global_handler

    def initialize(handlers, options = {})
      super(handlers, options)

      # only do the following if we're creating a new instance
      if !options.has_key?(:impl)
        @ssl = SSLConfig.new
        if options[:global_handler]
          self.global_handler = GlobalOverrides.new(options[:global_handler])
        else
          # very ugly, but using self.global_handler doesn't work in the constructor
          ghandler = Reactor.instance_method(:global_handler).bind(self).call
          ghandler = GlobalOverrides.new(ghandler)
          Reactor.instance_method(:global_handler=).bind(self).call(ghandler)
        end
        @trigger = nil
        @container_id = generate_uuid
      end
    end

    # Initiates the establishment of an AMQP connection.
    #
    # @param options [Hash] A hash of named arguments.
    #
    def connect(options = {})
      conn = self.connection(options[:handler])
      conn.container = self.container_id || generate_uuid
      connector = Connector.new(conn)
      conn.overrides = connector
      if !options[:url].nil?
        connector.address = URLs.new([options[:url]])
      elsif !options[:urls].nil?
        connector.address = URLs.new(options[:urls])
      elsif !options[:address].nil?
        connector.address = URLs.new([Qpid::Proton::URL.new(options[:address])])
      else
        raise ::ArgumentError.new("either :url or :urls or :address required")
      end

      connector.heartbeat = options[:heartbeat] if !options[:heartbeat].nil?
      if !options[:reconnect].nil?
        connector.reconnect = options[:reconnect]
      else
        connector.reconnect = Backoff.new()
      end

      connector.ssl_domain = SessionPerConnection.new # TODO seems this should be configurable

      conn.open

      return conn
    end

    def _session(context)
      if context.is_a?(Qpid::Proton::URL)
        return self._session(self.connect(:url => context))
      elsif context.is_a?(Qpid::Proton::Session)
        return context
      elsif context.is_a?(Qpid::Proton::Connection)
        if context.session_policy?
          return context.session_policy.session(context)
        else
          return self.create_session(context)
        end
      else
        return context.session
      end
    end

    # Initiates the establishment of a link over which messages can be sent.
    #
    # @param context [String, URL] The context.
    # @param opts [Hash] Additional options.
    # @param opts [String, Qpid::Proton::URL] The target address.
    # @param opts [String] :source The source address.
    # @param opts [Boolean] :dynamic
    # @param opts [Object] :handler
    # @param opts [Object] :tag_generator The tag generator.
    # @param opts [Hash] :options Addtional link options
    #
    # @return [Sender] The sender.
    #
    def create_sender(context, opts = {})
      if context.is_a?(::String)
        context = Qpid::Proton::URL.new(context)
      end

      target = opts[:target]
      if context.is_a?(Qpid::Proton::URL) && target.nil?
        target = context.path
      end

      session = self._session(context)

      sender = session.sender(opts[:name] ||
                              id(session.connection.container,
                                target, opts[:source]))
        sender.source.address = opts[:source] if !opts[:source].nil?
        sender.target.address = target if target
        sender.handler = opts[:handler] if !opts[:handler].nil?
        sender.tag_generator = opts[:tag_generator] if !opts[:tag_gnenerator].nil?
        self._apply_link_options(opts[:options], sender)
        sender.open
        return sender
    end

    # Initiates the establishment of a link over which messages can be received.
    #
    # There are two accepted arguments for the context
    #  1. If a Connection is supplied then the link is established using that
    # object. The source, and optionally the target, address can be supplied
    #  2. If it is a String or a URL then a new Connection is created on which
    # the link will be attached. If a path is specified, but not the source
    # address, then the path of the URL is used as the target address.
    #
    # The name will be generated for the link if one is not specified.
    #
    # @param context [Connection, URL, String] The connection or the address.
    # @param opts [Hash] Additional otpions.
    # @option opts [String, Qpid::Proton::URL] The source address.
    # @option opts [String] :target The target address
    # @option opts [String] :name The link name.
    # @option opts [Boolean] :dynamic
    # @option opts [Object] :handler
    # @option opts [Hash] :options Additional link options.
    #
    # @return [Receiver
    #
    def create_receiver(context, opts = {})
      if context.is_a?(::String)
        context = Qpid::Proton::URL.new(context)
      end

      source = opts[:source]
      if context.is_a?(Qpid::Proton::URL) && source.nil?
        source = context.path
      end

      session = self._session(context)

      receiver = session.receiver(opts[:name] ||
                                  id(session.connection.container,
                                      source, opts[:target]))
      receiver.source.address = source if source
      receiver.source.dynamic = true if opts.has_key?(:dynamic) && opts[:dynamic]
      receiver.target.address = opts[:target] if !opts[:target].nil?
      receiver.handler = opts[:handler] if !opts[:handler].nil?
      self._apply_link_options(opts[:options], receiver)
      receiver.open
      return receiver
    end

    def declare_transaction(context, handler = nil, settle_before_discharge = false)
      if context.respond_to? :txn_ctl && !context.__send__(:txn_ctl).nil?
        class << context
          attr_accessor :txn_ctl
        end
        context.txn_ctl = self.create_sender(context, nil, "txn-ctl",
        InternalTransactionHandler.new())
      end
      return Transaction.new(context.txn_ctl, handler, settle_before_discharge)
    end

    # Initiates a server socket, accepting incoming AMQP connections on the
    # interface and port specified.
    #
    # @param url []
    # @param ssl_domain []
    #
    def listen(url, ssl_domain = nil)
      url = Qpid::Proton::URL.new(url)
      acceptor = self.acceptor(url.host, url.port)
      ssl_config = ssl_domain
      if ssl_config.nil? && (url.scheme == 'amqps') && @ssl
        ssl_config = @ssl.server
      end
      if !ssl_config.nil?
        acceptor.ssl_domain(ssl_config)
      end
      return acceptor
    end

    def do_work(timeout = nil)
      self.timeout = timeout unless timeout.nil?
      self.process
    end

    def id(container, remote, local)
      if !local.nil? && !remote.nil?
        "#{container}-#{remote}-#{local}"
      elsif !local.nil?
        "#{container}-#{local}"
      elsif !remote.nil?
        "#{container}-#{remote}"
      else
        "#{container}-#{generate_uuid}"
      end
    end

    def _apply_link_options(options, link)
      if !options.nil? && !options.empty?
        if !options.is_a?(::List)
          options = [Options].flatten
        end

        options.each {|option| o.apply(link) if o.test(link)}
      end
    end

    def to_s
      "#{self.class}<@impl=#{Cproton.pni_address_of(@impl)}>"
    end

  end

end
