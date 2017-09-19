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

  private
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

  public
  # A representation of the AMQP concept of a container which, loosely
  # speaking, is something that establishes links to or from another
  # container on which messages are transferred.
  #
  # This is an extension to the Reactor classthat adds convenience methods
  # for creating instances of Qpid::Proton::Connection, Qpid::Proton::Sender
  # and Qpid::Proton::Receiver.
  class Container < Reactor

    include Qpid::Proton::Util::Reactor

    attr_accessor :container_id
    attr_accessor :global_handler

    def initialize(handlers, opts = {})
      super(handlers, opts)

      # only do the following if we're creating a new instance
      if !opts.has_key?(:impl)
        @container_id = String.new(opts[:container_id] || SecureRandom.uuid).freeze
        @ssl = SSLConfig.new
        if opts[:global_handler]
          self.global_handler = GlobalOverrides.new(opts[:global_handler])
        else
          # very ugly, but using self.global_handler doesn't work in the constructor
          ghandler = Reactor.instance_method(:global_handler).bind(self).call
          ghandler = GlobalOverrides.new(ghandler)
          Reactor.instance_method(:global_handler=).bind(self).call(ghandler)
        end
        @trigger = nil
      end
    end

    # Initiate an AMQP connection.
    #
    # @param url [String] Connect to URL host:port, using user:password@ if present
    # @param opts [Hash] Named options
    #   For backwards compatibility, can be called with a single parameter opts.
    #
    # @option opts [String] :url Connect to URL host:port using user:password@ if present.
    # @option opts [String] :user user name for authentication if not given by URL
    # @option opts [String] :password password for authentication if not given by URL
    # @option opts [Numeric] :idle_timeout seconds before closing an idle connection,
    #   can be a fractional value.
    # @option opts [Boolean] :sasl_enabled Enable or disable SASL.
    # @option opts [Boolean] :sasl_allow_insecure_mechs Allow mechanisms that disclose clear text
    #   passwords, even over an insecure connection. By default, such mechanisms are only allowed
    #   when SSL is enabled.
    # @option opts [String] :sasl_allowed_mechs the allowed SASL mechanisms for use on the connection.
    #
    # @option opts [String] :address *deprecated* use the :url option
    # @option opts [Numeric] :heartbeat milliseconds before closing an idle connection.
    #   *deprecated* use :idle_timeout => heartbeat/1000 
    #
    # @return [Connection] the new connection
    #
    def connect(url, opts = {})
      # Backwards compatible with old connect(opts)
      if url.is_a? Hash and opts.empty?
        opts = url
        url = nil
      end
      conn = self.connection(opts[:handler])
      connector = Connector.new(conn, url, opts)
      return conn
    end

    # Initiates the establishment of a link over which messages can be sent.
    #
    # @param context [String, URL] The context.
    # @param opts [Hash] Additional opts.
    # @param opts [String] :target The target address.
    # @param opts [String] :source The source address.
    # @param opts [Boolean] :dynamic
    # @param opts [Object] :handler
    #
    # @return [Sender] The sender.
    #
    def open_sender(context, opts = {})
      if context.is_a?(::String)
        context = Qpid::Proton::URL.new(context)
      end
      if context.is_a?(Qpid::Proton::URL)
        opts[:target] ||= context.path
      end

      return _session(context).open_sender(opts)
    end

    # @deprecated use @{#open_sender}
    alias_method :create_sender, :open_sender

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
    # @param context [Connection, URL, String] The connection or the connection address.
    # @param opts [Hash] Additional opts.
    # @option opts [String] :source The source address.
    # @option opts [String] :target The target address
    # @option opts [String] :name The link name.
    # @option opts [Boolean] :dynamic
    # @option opts [Object] :handler
    #
    # @return [Receiver]
    #
    def open_receiver(context, opts = {})
      if context.is_a?(::String)
        context = Qpid::Proton::URL.new(context)
      end
      if context.is_a?(Qpid::Proton::URL)
        opts[:source] ||= context.path
      end
      return _session(context).open_receiver(opts)
    end

    # @deprecated use @{#open_sender}
    alias_method :create_receiver, :open_receiver

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

    private

    def _session(context)
      if context.is_a?(Qpid::Proton::URL)
        return _session(self.connect(:url => context))
      elsif context.is_a?(Qpid::Proton::Session)
        return context
      elsif context.is_a?(Qpid::Proton::Connection)
        return context.default_session
      else
        return context.session
      end
    end

    def do_work(timeout = nil)
      self.timeout = timeout unless timeout.nil?
      self.process
    end

    def _apply_link_opts(opts, link)
      opts.each {|o| o.apply(link) if o.test(link)}
    end

    def to_s
      "#{self.class}<@impl=#{Cproton.pni_address_of(@impl)}>"
    end

  end

end
