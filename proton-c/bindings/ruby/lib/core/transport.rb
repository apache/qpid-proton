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

  # A transport is used by a connection to interface with the network.
  #
  # A transport is associated with, at most, one Connection.
  #
  # == Client And Server Mode
  #
  # Initially, a transport is configured to be a client tranpsort. It can be
  # configured to act as a server when it is created.
  #
  # A client transport initiates outgoing connections.
  #
  # A client transport must be configured with the protocol layers to use and
  # cannot configure itself automatically.
  #
  # A server transport accepts incoming connections. It can automatically
  # configure itself to include the various protocol layers depending on the
  # incoming protocol headers.
  #
  # == Tracing Data
  #
  # Data can be traced into and out of the transport programmatically by setting
  # the #trace level to one of the defined trace values (TRACE_RAW, TRACE_FRM or
  # TRACE_DRV). Tracing can also be turned off programmatically by setting the
  # #trace level to TRACE_OFF.
  #
  # @example
  #
  #   # turns on frame tracing
  #   @transport.trace = Qpid::Proton::Transport::TRACE_FRM
  #
  #   # ... do something where the frames are of interest, such as debugging
  #
  #   # turn tracing off again
  #   @transport.trace = Qpid::Proton::Transport::TRACE_NONE
  #
  # Tracing can also be enabled from the command line by defining the similarly
  # named environment variable before starting a Proton application:
  #
  # @example
  #
  #   # enable tracing from the command line
  #   PN_TRACE_FRM=1 ruby my_proton_app.rb
  #
  class Transport

    # @private
    include Util::Engine

    # Turn logging off entirely.
    TRACE_OFF = Cproton::PN_TRACE_OFF
    # Log raw binary data into/out of the transport.
    TRACE_RAW = Cproton::PN_TRACE_RAW
    # Log frames into/out of the transport.
    TRACE_FRM = Cproton::PN_TRACE_FRM
    # Log driver related events; i.e., initialization, end of stream, etc.
    TRACE_DRV = Cproton::PN_TRACE_DRV

    # @private
    CLIENT = 1
    # @private
    SERVER = 2

    # @private
    include Util::SwigHelper

    # @private
    PROTON_METHOD_PREFIX = "pn_transport"

    # @!attribute channel_max
    #
    # @return [Fixnum] The maximum allowed channel.
    #
    proton_accessor :channel_max

    # @!attribute [r] remote_channel_max
    #
    # @return [Fixnum] The maximum allowed channel of a transport's remote peer.
    #
    proton_caller :remote_channel_max

    # @!attribute max_frame_size
    #
    # @return [Fixnum] The maximum frame size.
    #
    proton_accessor :max_frame_size

    # @!attribute [r] remote_max_frame_size
    #
    # @return [Fixnum] The maximum frame size of the transport's remote peer.
    #
    proton_reader :remote_max_frame_size

    # @!attribute idle_timeout
    #
    # @return [Fixnum] The idle timeout.
    #
    proton_accessor :idle_timeout

    # @!attribute [r] remote_idle_timeout
    #
    # @return [Fixnum] The idle timeout for the transport's remote peer.
    #
    proton_accessor :remote_idle_timeout

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
    # @return [Fixnum] The amount of free space for input following the
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
    # @return [Fixnum] The number of pending output bytes following the header
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
    # @return [Fixnum] The number of frames output by a transport.
    #
    proton_reader :frames_output

    # @!attribute [r] frames_input
    #
    # @return [Fixnum] The number of frames input by a transport.
    #
    proton_reader :frames_input

    # @private
    include Util::ErrorHandler

    can_raise_error :process, :error_class => TransportError
    can_raise_error :close_tail, :error_class => TransportError
    can_raise_error :pending, :error_class => TransportError, :below => Error::EOS
    can_raise_error :close_head, :error_class => TransportError

    # @private
    include Util::Wrapper

    # @private
    def self.wrap(impl)
      return nil if impl.nil?

      self.fetch_instance(impl, :pn_transport_attachments) || Transport.new(nil, impl)
    end

    # Creates a new transport instance.
    #
    # @param mode [Fixnum] The transport mode, either CLIENT or SERVER
    # @param impl [pn_transport_t] Should not be used.
    #
    # @raise [TransportError] If the mode is invalid.
    #
    def initialize(mode = nil, impl = Cproton.pn_transport)
      @impl = impl
      if mode == SERVER
        Cproton.pn_transport_set_server(@impl)
      elsif (!mode.nil? && mode != CLIENT)
        raise TransportError.new("cannot create transport for mode: #{mode}")
      end
      self.class.store_instance(self, :pn_transport_attachments)
    end

    # Returns whether the transport has any buffered data.
    #
    # @return [Boolean] True if the transport has no buffered data.
    #
    def quiesced?
      Cproton.pn_transport_quiesced(@impl)
    end

    # Returns additional information about the condition of the transport.
    #
    # When a TRANSPORT_ERROR event occurs, this operaiton can be used to
    # access the details of the error condition.
    #
    # The object returned is valid until the Transport is discarded.
    #
    def condition
      condition_to_object Cproton.pn_transport_condition(@impl)
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
    # @param level [Fixnum] The trace level.
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
    # @return [Fixnum] The number of bytes pushed.
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
    # @param size [Fixnum] The number of bytes to process.
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
    # @param size [Fixnum] The number of bytes to return.
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
    # @param size [Fixnum] The number of bytes to remove.
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
    # @return [Fixnum] If non-zero, the expiration time of the next pending
    #   timer event for the transport. The caller must invoke #tick again at
    #   least once at or before this deadline occurs.
    #
    def tick(now)
      Cproton.pn_transport_tick(@impl, now)
    end

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
      @ssl ||= SSL.create(self, domain, session_details) if @ssl.nil?
    end

    # @private
    def ssl?
      !@ssl.nil?
    end

  end

end
