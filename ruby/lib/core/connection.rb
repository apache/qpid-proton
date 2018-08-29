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

  # An AMQP connection.
  class Connection < Endpoint
    PROTON_METHOD_PREFIX = "pn_connection"
    include Util::Wrapper
    include Util::Deprecation

    # @private
    def self.wrap(impl)
      return nil if impl.nil?
      self.fetch_instance(impl, :pn_connection_attachments) || Connection.new(impl)
    end

    # @private
    def initialize(impl = Cproton.pn_connection)
      super()
      @impl = impl
      @overrides = nil
      @session_policy = nil
      @link_count = 0
      @link_prefix = ""
      self.class.store_instance(self, :pn_connection_attachments)
    end

    # @return [String] The AMQP hostname for the connection.
    def virtual_host() Cproton.pn_connection_remote_hostname(@impl); end
    deprecated_alias :remote_hostname, :virtual_host

    # @!attribute hostname
    # @deprecated use {#virtual_host}
    proton_set_get :hostname

    # @return [String] User name used for authentication (outgoing connection)
    # or the authenticated user name (incoming connection)
    def user()
      Cproton.pn_connection_get_user(impl) or (connection.transport && connection.transport.user)
    end

    # @deprecated no replacement
    def overrides?() deprecated __method__; false; end

    # @deprecated no replacement
    def session_policy?() deprecated __method__; false; end

    # @return [Connection] self
    def connection() self; end

    # @return [Transport, nil] transport bound to this connection, or nil if unbound.
    def transport() Transport.wrap(Cproton.pn_connection_transport(@impl)); end

    # @return AMQP container ID advertised by the remote peer.
    # To get the local container ID use {#container} and {Container#id}
    def container_id() Cproton.pn_connection_remote_container(@impl); end
    deprecated_alias :remote_container, :container_id

    # @return [Container] the container managing this connection
    attr_reader :container

    # @return [Array<Symbol>] offered capabilities provided by the remote peer
    def offered_capabilities
      # Provide capabilities consistently as an array, even if encoded as a single symbol
      Codec::Data.to_multiple(Cproton.pn_connection_remote_offered_capabilities(@impl))
    end
    deprecated_alias :remote_offered_capabilities, :offered_capabilities

    # @return [Array<Symbol>] desired capabilities provided by the remote peer
    def desired_capabilities
      # Provide capabilities consistently as an array, even if encoded as a single symbol
      Codec::Data.to_multiple(Cproton.pn_connection_remote_desired_capabilities(@impl))
    end
    deprecated_alias :remote_desired_capabilities, :desired_capabilities

    # @return [Hash] connection-properties provided by the remote peer
    def properties
      Codec::Data.to_object(Cproton.pn_connection_remote_properties(@impl))
    end
    deprecated_alias :remote_properties, :properties

    # Open the local end of the connection.
    #
    # @option opts [MessagingHandler] :handler handler for events related to this connection.
    #
    # @option opts [String] :user User name for authentication
    # @option opts [String] :password Authentication secret
    # @option opts [String] :virtual_host Virtual host name
    # @option opts [String] :container_id (provided by {Container}) override advertised container-id
    #
    # @option opts [Hash<Symbol=>Object>] :properties Application-defined properties
    # @option opts [Array<Symbol>] :offered_capabilities Extensions the endpoint supports
    # @option opts [Array<Symbol>] :desired_capabilities Extensions the endpoint can use
    #
    # @option opts [Numeric] :idle_timeout Seconds before closing an idle connection
    # @option opts [Integer] :max_sessions Limit the number of active sessions
    # @option opts [Integer] :max_frame_size Limit the size of AMQP frames
    #
    # @option opts [Boolean] :sasl_enabled (false) Enable or disable SASL.
    # @option opts [Boolean] :sasl_allow_insecure_mechs (false) Allow mechanisms that send secrets in cleartext
    # @option opts [String] :sasl_allowed_mechs Specify the SASL mechanisms allowed for this connection.  The value is a space-separated list of mechanism names. The mechanisms allowed by default are determined by your SASL library and system configuration, with two exceptions: GSSAPI and GSS-SPNEGO are disabled by default.  To enable them, you must explicitly add them using this option. Clients must set the allowed mechanisms before the the outgoing connection is attempted.  Servers must set them before the listening connection is setup.
    #
    # @option opts [SSLDomain] :ssl_domain SSL configuration domain.
    #
    def open(opts=nil)
      return if local_active?
      apply opts if opts
      Cproton.pn_connection_open(@impl)
    end

    # @private
    def apply opts
      # NOTE: Only connection options are set here.
      # Transport options must be applied with {Transport#apply}
      @container = opts[:container]
      cid = opts[:container_id] || (@container && @container.id) || SecureRandom.uuid
      cid = cid.to_s if cid.is_a? Symbol # Allow symbols as container name
      Cproton.pn_connection_set_container(@impl, cid)
      Cproton.pn_connection_set_user(@impl, opts[:user]) if opts[:user]
      Cproton.pn_connection_set_password(@impl, opts[:password]) if opts[:password]
      Cproton.pn_connection_set_hostname(@impl, opts[:virtual_host]) if opts[:virtual_host]
      @link_prefix = opts[:link_prefix] || cid
      Codec::Data.from_object(Cproton.pn_connection_offered_capabilities(@impl), opts[:offered_capabilities])
      Codec::Data.from_object(Cproton.pn_connection_desired_capabilities(@impl), opts[:desired_capabilities])
      Codec::Data.from_object(Cproton.pn_connection_properties(@impl), opts[:properties])
    end

    # Idle-timeout advertised by the remote peer, in seconds.
    # @return [Numeric] Idle-timeout advertised by the remote peer, in seconds.
    # @return [nil] if the peer does not advertise an idle time-out
    def idle_timeout()
      if transport && (t = transport.remote_idle_timeout)
        Rational(t, 1000)       # More precise than Float
      end
    end

    # Session limit advertised by the remote peer. See {Connection#open :max_sessions}
    # @return [Integer] maximum number of sessions per connection allowed by remote peer.
    # @return [nil] no specific limit is set.
    def max_sessions()
      raise StateError, "connection not bound to transport" unless transport
      max = transport.remote_channel_max
      return max.zero? ? nil : max
    end

    # Maximum frame size, in bytes, advertised by the remote peer.
    # See {Connection#open :max_frame_size}
    # @return [Integer] maximum frame size
    # @return [nil] no limit
    def max_frame_size()
      raise StateError, "connection not bound to transport" unless transport
      max = transport.remote_max_frame
      return max.zero? ? nil : max
    end

    # Closes the local end of the connection. The remote end may or may not be closed.
    # @param error [Condition] Optional error condition to send with the close.
    def close(error=nil)
      Condition.assign(_local_condition, error)
      Cproton.pn_connection_close(@impl)
    end

    # Gets the endpoint current state flags
    #
    # @see Endpoint#LOCAL_UNINIT
    # @see Endpoint#LOCAL_ACTIVE
    # @see Endpoint#LOCAL_CLOSED
    # @see Endpoint#LOCAL_MASK
    #
    # @return [Integer] The state flags.
    #
    def state
      Cproton.pn_connection_state(@impl)
    end

    # Returns the default session for this connection.
    #
    # @return [Session] The session.
    #
    def default_session
      @session ||= open_session
    end

    # @deprecated use #default_session()
    deprecated_alias :session, :default_session

    # Open a new session on this connection.
    def open_session
      s = Session.wrap(Cproton.pn_session(@impl))
      s.open
      return s
    end

    # Open a sender on the default_session
    # @option opts (see Session#open_sender)
    def open_sender(opts=nil) default_session.open_sender(opts) end

    # Open a  on the default_session
    # @option opts (see Session#open_receiver)
    def open_receiver(opts=nil) default_session.open_receiver(opts) end

    # @deprecated use {#each_session}
    def  session_head(mask)
      deprecated __method__, "#each_session"
      Session.wrap(Cproton.pn_session_head(@impl, mask))
    end

    # Get the sessions on this connection.
    # @overload each_session
    #   @yieldparam s [Session] pass each session to block
    # @overload each_session
    #   @return [Enumerator] enumerator over sessions
    def each_session(&block)
      return enum_for(:each_session) unless block_given?
      s = Cproton.pn_session_head(@impl, 0);
      while s
        yield Session.wrap(s)
        s = Cproton.pn_session_next(s, 0)
      end
      self
    end

    # @deprecated use {#each_link}
    def link_head(mask)
      deprecated __method__, "#each_link"
      Link.wrap(Cproton.pn_link_head(@impl, mask))
    end

    # Get the links on this connection.
    # @overload each_link
    #   @yieldparam l [Link] pass each link to block
    # @overload each_link
    #   @return [Enumerator] enumerator over links
    def each_link
      return enum_for(:each_link) unless block_given?
      l = Cproton.pn_link_head(@impl, 0);
      while l
        l2 = l                  #  get next before yield, in case yield closes l and unlinks it
        l = Cproton.pn_link_next(l, 0)
        yield Link.wrap(l2)
      end
      self
    end

    # Get the {Sender} links - see {#each_link}
    def each_sender()
      return enum_for(:each_sender) unless block_given?
      each_link.select { |l| yield l if l.sender? }
    end

    # Get the {Receiver} links - see {#each_link}
      def each_receiver()
        return enum_for(:each_receiver) unless block_given?
        each_link.select { |l| yield l if l.receiver? }
      end

    # @deprecated use {#MessagingHandler} to handle work
    def work_head
      deprecated __method__
      Delivery.wrap(Cproton.pn_work_head(@impl))
    end

    # @deprecated use {#condition}
    def error
      deprecated __method__, "#condition"
      Cproton.pn_error_code(Cproton.pn_connection_error(@impl))
    end

    # @private Generate a unique link name, internal use only.
    def link_name()
      @link_prefix + "/" +  (@link_count += 1).to_s(32)
    end

    # @return [WorkQueue] work queue to execute code serialized correctly for this connection
    attr_reader :work_queue

    protected

    def _local_condition
      Cproton.pn_connection_condition(@impl)
    end

    def _remote_condition
      Cproton.pn_connection_remote_condition(@impl)
    end

    proton_get :attachments
  end
end
