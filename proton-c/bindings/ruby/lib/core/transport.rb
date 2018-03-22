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

  # @deprecated all important features are available from {#Connection}
  class Transport
    include Util::Deprecation

    # @private
    PROTON_METHOD_PREFIX = "pn_transport"
    # @private
    include Util::Wrapper

    # Turn logging off entirely.
    TRACE_OFF = Cproton::PN_TRACE_OFF
    # Log raw binary data into/out of the transport.
    TRACE_RAW = Cproton::PN_TRACE_RAW
    # Log frames into/out of the transport.
    TRACE_FRM = Cproton::PN_TRACE_FRM
    # Log driver related events; i.e., initialization, end of stream, etc.
    TRACE_DRV = Cproton::PN_TRACE_DRV

    proton_get :user

    # @!attribute channel_max
    #
    # @return [Integer] The maximum allowed channel.
    #
    proton_set_get :channel_max

    # @!attribute [r] remote_channel_max
    #
    # @return [Integer] The maximum allowed channel of a transport's remote peer.
    #
    proton_caller :remote_channel_max

    # @!attribute max_frame_size
    #
    # @return [Integer] The maximum frame size.
    proton_set_get :max_frame
    proton_get :remote_max_frame

    # @!attribute [r] remote_max_frame_size
    #
    # @return [Integer] The maximum frame size of the transport's remote peer.
    #
    proton_get :remote_max_frame_size

    # @!attribute idle_timeout
    #
    # @deprecated use {Connection#open} with the +:idle_timeout+ option to set
    # the timeout, and {Connection#idle_timeout} to query the remote timeout.
    #
    # The Connection timeout values are in *seconds* and are automatically
    # converted.
    #
    # @return [Integer] The idle timeout in *milliseconds*.
    #
    proton_set_get :idle_timeout

    # @!attribute [r] remote_idle_timeout in milliseconds
    #
    # @deprecated Use {Connection#idle_timeout} to query the remote timeout.
    #
    # @return [Integer] The idle timeout for the transport's remote peer.
    #
    proton_set_get :remote_idle_timeout

    # @!attribute [r] capacity
    #
    # If the engine is in an exception state such as encountering an error
    # condition or reaching the end of stream state, a negative value will
    # be returned indicating the condition.
    #
    # If an error is indicated, further deteails can be obtained from
    # #error.
    #
    # Calls to #process may alter the value of this value. See #process for
    # more details
    #
    # @return [Integer] The amount of free space for input following the
    # transport's tail pointer.
    #
    proton_caller :capacity

    # @!attribute [r] head
    #
    # This referneces queued output data. It reports the bytes of output data.
    #
    # Calls to #pop may alter this attribute, and any data it references.
    #
    # @return [String] The transport's head pointer.
    #
    proton_caller :head

    # @!attribute [r] tail
    #
    # The amount of free space following this data is reported by #capacity.
    #
    # Calls to #process may alter the value of this attribute.
    #
    # @return [String] The transport's tail pointer.
    #
    proton_caller :tail

    # @!attribute [r] pending
    #
    # If the ending is in an exceptional state, such as encountering an error
    # condition or reachign the end of the stream state, a negative value will
    # be returned indicating the condition.
    #
    # If an error is indicated, further details can be obtained from #error.
    #
    # Calls to #pop may alter the value of this pointer as well.
    #
    # @return [Integer] The number of pending output bytes following the header
    # pointer.
    #
    # @raise [TransportError] If any error other than an end of stream occurs.
    #
    proton_caller :pending

    # @!attribute [r] closed?
    #
    # A transport is defined to be closed when both the tail and the head are
    # closed. In other words, when both #capacity < 0 and #pending < 0.
    #
    # @return [Boolean] Returns true if the tranpsort is closed.
    #
    proton_caller :closed?

    # @!attribute [r] frames_output
    #
    # @return [Integer] The number of frames output by a transport.
    #
    proton_get :frames_output

    # @!attribute [r] frames_input
    #
    # @return [Integer] The number of frames input by a transport.
    #
    proton_get :frames_input

    # @private
    include Util::ErrorHandler

    # @private
    def self.wrap(impl)
      return nil if impl.nil?

      self.fetch_instance(impl, :pn_transport_attachments) || Transport.new(impl)
    end

    # Creates a new transport instance.
    def initialize(impl = Cproton.pn_transport)
      @impl = impl
      @ssl = nil
      self.class.store_instance(self, :pn_transport_attachments)
    end

    # Set server mode for this tranport - enables protocol detection
    # and server-side authentication for incoming connections
    def set_server() Cproton.pn_transport_set_server(@impl); end

    # Returns whether the transport has any buffered data.
    #
    # @return [Boolean] True if the transport has no buffered data.
    #
    def quiesced?
      Cproton.pn_transport_quiesced(@impl)
    end

    # @return [Condition, nil] transport error condition or nil if there is no error.
    def condition
      Condition.convert(Cproton.pn_transport_condition(@impl))
    end

    # Set the error condition for the transport.
    # @param c [Condition] The condition to set
    def condition=(c)
      Condition.assign(Cproton.pn_transport_condition(@impl), c)
    end

    # Binds to the given connection.
    #
    # @param connection [Connection] The connection.
    #
    def bind(connection)
      Cproton.pn_transport_bind(@impl, connection.impl)
    end

    # Unbinds from the previous connection.
    #
    def unbind
      Cproton.pn_transport_unbind(@impl)
    end

    # Updates the transports trace flags.
    #
    # @param level [Integer] The trace level.
    #
    # @see TRACE_OFF
    # @see TRACE_RAW
    # @see TRACE_FRM
    # @see TRACE_DRV
    #
    def trace(level)
      Cproton.pn_transport_trace(@impl, level)
    end

    # Return the AMQP connection associated with the transport.
    #
    # @return [Connection, nil] The bound connection, or nil.
    #
    def connection
      Connection.wrap(Cproton.pn_transport_connection(@impl))
    end

    # Log a message to the transport's logging mechanism.
    #
    # This can be using in a debugging scenario as the message will be
    # prepended with the transport's identifier.
    #
    # @param message [String] The message to be logged.
    #
    def log(message)
      Cproton.pn_transport_log(@impl, message)
    end

    # Pushes the supplied bytes into the tail of the transport.
    #
    # @param data [String] The bytes to be pushed.
    #
    # @return [Integer] The number of bytes pushed.
    #
    def push(data)
      Cproton.pn_transport_push(@impl, data, data.length)
    end

    # Process input data following the tail pointer.
    #
    # Calling this function will cause the transport to consume the specified
    # number of bytes of input occupying the free space following the tail
    # pointer. It may also change the value for #tail, as well as the amount of
    # free space reported by #capacity.
    #
    # @param size [Integer] The number of bytes to process.
    #
    # @raise [TransportError] If an error occurs.
    #
    def process(size)
      Cproton.pn_transport_process(@impl, size)
    end

    # Indicate that the input has reached EOS (end of stream).
    #
    # This tells the transport that no more input will be forthcoming.
    #
    # @raise [TransportError] If an error occurs.
    #
    def close_tail
      Cproton.pn_transport_close_tail(@impl)
    end

    # Returns the specified number of bytes from the transport's buffers.
    #
    # @param size [Integer] The number of bytes to return.
    #
    # @return [String] The data peeked.
    #
    # @raise [TransportError] If an error occurs.
    #
    def peek(size)
      cd, out = Cproton.pn_transport_peek(@impl, size)
      return nil if cd == Qpid::Proton::Error::EOS
      raise TransportError.new if cd < -1
      out
    end

    # Removes the specified number of bytes from the pending output queue
    # following the transport's head pointer.
    #
    # @param size [Integer] The number of bytes to remove.
    #
    def pop(size)
      Cproton.pn_transport_pop(@impl, size)
    end

    # Indicate that the output has closed.
    #
    # Tells the transport that no more output will be popped.
    #
    # @raise [TransportError] If an error occurs.
    #
    def close_head
      Cproton.pn_transport_close_head(@impl)
    end

    # Process any pending transport timer events.
    #
    # This method should be called after all pending input has been
    # processed by the transport (see #input), and before generating
    # output (see #output).
    #
    # It returns the deadline for the next pending timer event, if any
    # art present.
    #
    # @param now [Time] The timestamp.
    #
    # @return [Integer] If non-zero, the expiration time of the next pending
    #   timer event for the transport. The caller must invoke #tick again at
    #   least once at or before this deadline occurs.
    #
    def tick(now)
      Cproton.pn_transport_tick(@impl, now)
    end

    # Create, or return existing, SSL object for the transport.
    # @return [SASL] the SASL object
    def sasl
      SASL.new(self)
    end

    # Creates, or returns an existing, SSL object for the transport.
    #
    # @param domain [SSLDomain] The SSL domain.
    # @param session_details [SSLDetails] The SSL session details.
    #
    # @return [SSL] The SSL object.
    #
    def ssl(domain = nil, session_details = nil)
      @ssl ||= SSL.create(self, domain, session_details)
    end

    # @private
    def ssl?
      !@ssl.nil?
    end

    # @private
    # Options are documented {Connection#open}, keep that consistent with this
    def apply opts
      sasl if opts[:sasl_enabled]                                 # Explicitly enabled
      unless opts.include?(:sasl_enabled) && !opts[:sasl_enabled] # Not explicitly disabled
        sasl.allowed_mechs = opts[:sasl_allowed_mechs] if opts.include? :sasl_allowed_mechs
        sasl.allow_insecure_mechs = opts[:sasl_allow_insecure_mechs] if opts.include? :sasl_allow_insecure_mechs
      end
      self.channel_max= opts[:max_sessions] if opts.include? :max_sessions
      self.max_frame = opts[:max_frame_size] if opts.include? :max_frame_size
      # NOTE: The idle_timeout option is in Numeric *seconds*, can be Integer, Float or Rational.
      # This is consistent with idiomatic ruby.
      # The transport #idle_timeout property is in *milliseconds* passed direct to C.
      # Direct use of the transport is deprecated.
      self.idle_timeout= (opts[:idle_timeout]*1000).round if opts.include? :idle_timeout
      self.ssl(opts[:ssl_domain]) if opts[:ssl_domain]
    end

    can_raise_error :process, :error_class => TransportError
    can_raise_error :close_tail, :error_class => TransportError
    can_raise_error :pending, :error_class => TransportError, :below => Error::EOS
    can_raise_error :close_head, :error_class => TransportError
  end
end
