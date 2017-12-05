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

  # An AMQP connection.
  class Connection < Endpoint

    protected
    include Util::SwigHelper
    PROTON_METHOD_PREFIX = "pn_connection"

    public

    # @!attribute hostname
    #   @return [String] The AMQP hostname for the connection.
    proton_accessor :hostname

    # @!attribute user
    #   @return [String] User name used for authentication (outgoing connection) or the authenticated user name (incoming connection)
    proton_accessor :user

    private

    proton_writer :password
    attr_accessor :overrides
    attr_accessor :session_policy
    include Util::Wrapper

    def self.wrap(impl)
      return nil if impl.nil?
      self.fetch_instance(impl, :pn_connection_attachments) || Connection.new(impl)
    end

    def initialize(impl = Cproton.pn_connection)
      super()
      @impl = impl
      @overrides = nil
      @session_policy = nil
      @link_count = 0
      @link_prefix = ""
      self.class.store_instance(self, :pn_connection_attachments)
    end

    public

    # @deprecated no replacement
    def overrides?() Qpid.deprecated __method__; false; end

    # @deprecated no replacement
    def session_policy?() Qpid.deprecated __method__; false; end

    # @return [Connection] self
    def connection() self; end

    # @return [Transport, nil] transport bound to this connection, or nil if unbound.
    #
    def transport() Transport.wrap(Cproton.pn_connection_transport(@impl)); end

    # @return AMQP container ID advertised by the remote peer
    def remote_container_id() Cproton.pn_connection_remote_container(@impl); end

    alias remote_container remote_container_id

    # @return [Container] the container managing this connection
    attr_reader :container

    # @return AMQP container ID for the local end of the connection
    def container_id() Cproton.pn_connection_get_container(@impl); end

    # @return [String] hostname used by the remote end of the connection
    def remote_hostname() Cproton.pn_connection_remote_hostname(@impl); end

    # @return [Array<Symbol>] offered capabilities provided by the remote peer
    def remote_offered_capabilities
      Codec::Data.to_object(Cproton.pn_connection_remote_offered_capabilities(@impl))
    end

    # @return [Array<Symbol>] desired capabilities provided by the remote peer
    def remote_desired_capabilities
      Codec::Data.to_object(Cproton.pn_connection_remote_desired_capabilities(@impl))
    end

    # @return [Hash] connection-properties provided by the remote peer
    def remote_properties
      Codec::Data.to_object(Cproton.pn_connection_remote_properites(@impl))
    end

    # Open the local end of the connection.
    #
    # @option opts [MessagingHandler] :handler handler for events related to this connection.
    # @option opts [String] :user user-name for authentication.
    # @option opts [String] :password password for authentication.
    # @option opts [Numeric] :idle_timeout seconds before closing an idle connection
    # @option opts [Boolean] :sasl_enabled Enable or disable SASL.
    # @option opts [Boolean] :sasl_allow_insecure_mechs Allow mechanisms that disclose clear text
    #   passwords, even over an insecure connection.
    # @option opts [String] :sasl_allowed_mechs the allowed SASL mechanisms for use on the connection.
    # @option opts [String] :container_id AMQP container ID, normally provided by {Container}
    #
    def open(opts=nil)
      return if local_active?
      apply opts if opts
      Cproton.pn_connection_open(@impl)
    end

    # @private
    def apply opts
      # NOTE: Only connection options are set here. Transport options are set
      # with {Transport#apply} from the connection_driver (or in
      # on_connection_bound if not using a connection_driver)
      @container = opts[:container]
      cid = opts[:container_id] || (@container && @container.id) || SecureRandom.uuid
      Cproton.pn_connection_set_container(@impl, cid)
      Cproton.pn_connection_set_user(@impl, opts[:user]) if opts[:user]
      Cproton.pn_connection_set_password(@impl, opts[:password]) if opts[:password]
      @link_prefix = opts[:link_prefix] || container_id
      Codec::Data.from_object(Cproton.pn_connection_offered_capabilities(@impl), opts[:offered_capabilities])
      Codec::Data.from_object(Cproton.pn_connection_desired_capabilities(@impl), opts[:desired_capabilities])
      Codec::Data.from_object(Cproton.pn_connection_properties(@impl), opts[:properties])
    end

    # @private Generate a unique link name, internal use only.
    def link_name()
      @link_prefix + "/" +  (@link_count += 1).to_s(16)
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
    alias session default_session

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

    # Returns the first session from the connection that matches the specified
    # state mask.
    #
    # Examines the state of each session owned by the connection, and returns
    # the first session that matches the given state mask. If the state mask
    # contains *both* local and remote flags, then an exact match against
    # those flags is performed. If the state mask contains only local *or*
    # remote flags, then a match occurs if a*any* of the local or remote flags
    # are set, respectively.
    #
    # @param mask [Integer] The state mask to be matched.
    #
    # @return [Session] The first matching session, or nil if none matched.
    #
    # @see Endpoint#LOCAL_UNINIT
    # @see Endpoint#LOCAL_ACTIVE
    # @see Endpoint#LOCAL_CLOSED
    # @see Endpoint#REMOTE_UNINIT
    # @see Endpoint#REMOTE_ACTIVE
    # @see Endpoint#REMOTE_CLOSED
    #
    def session_head(mask)
      Session.wrap(Cproton.pn_session_header(@impl, mask))
    end

    # Returns the first link that matches the given state mask.
    #
    # Examines the state of each link owned by the connection and returns the
    # first that matches the given state mask. If the state mask contains
    # *both* local and remote flags, then an exact match against those flags
    # is performed. If the state mask contains *only* local or remote flags,
    # then a match occurs if *any* of the local ore remote flags are set,
    # respectively.
    #
    # @param mask [Integer] The state mask to be matched.
    #
    # @return [Link] The first matching link, or nil if none matched.
    #
    # @see Endpoint#LOCAL_UNINIT
    # @see Endpoint#LOCAL_ACTIVE
    # @see Endpoint#LOCAL_CLOSED
    # @see Endpoint#REMOTE_UNINIT
    # @see Endpoint#REMOTE_ACTIVE
    # @see Endpoint#REMOTE_CLOSED
    #
    def link_head(mask)
      Link.wrap(Cproton.pn_link_head(@impl, mask))
    end

    # Extracts the first delivery on the connection that has pending
    # operations.
    #
    # A readable delivery indicates message data is waiting to be read. A
    # A writable delivery indcates that message data may be sent. An updated
    # delivery indicates that the delivery's disposition has changed.
    #
    # A delivery will never be *both* readable and writable, but it may be
    # both readable or writable and updated.
    #
    # @return [Delivery] The delivery, or nil if none are available.
    #
    # @see Delivery#next
    #
    def work_head
      Delivery.wrap(Cproton.pn_work_head(@impl))
    end

    # Returns the code for a connection error.
    #
    # @return [Integer] The error code.
    #
    def error
      Cproton.pn_error_code(Cproton.pn_connection_error(@impl))
    end

    protected

    def _local_condition
      Cproton.pn_connection_condition(@impl)
    end

    def _remote_condition
      Cproton.pn_connection_remote_condition(@impl)
    end

    proton_reader :attachments
  end
end
