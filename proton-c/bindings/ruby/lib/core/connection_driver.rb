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

require 'socket'

module Qpid
  module Proton

    # Associate an AMQP {Connection} with an {IO} and a {MessagingHandler}
    #
    # - Read AMQP binary data from the {IO} (#read, #process)
    # - Call on_* methods on the {MessagingHandler} for AMQP events (#dispatch, #process)
    # - Write AMQP binary data to the {IO} (#write, #process)
    #
    # Thread safety: The {ConnectionDriver} is not thread safe but separate
    # {ConnectionDriver} instances can be processed concurrently. The
    # {Container} handles multiple connections concurrently in multiple threads.
    #
    class ConnectionDriver

      # Create a {Connection} and associate it with +io+ and +handler+
      #
      # @param io [#read_nonblock, #write_nonblock] An {IO} or {IO}-like object that responds
      #   to #read_nonblock and #write_nonblock.
      # @param opts [Hash] See {Connection#open} - transport options are set here,
      # remaining options
      # @pram server [Bool] If true create a server (incoming) connection
      def initialize(io, opts = {}, server=false)
        @impl = Cproton.pni_connection_driver or raise RuntimeError, "cannot create connection driver"
        @io = io
        @handler = opts[:handler] || Handler::MessagingHandler.new # Default handler if missing
        @rbuf = ""              # String to re-use as read buffer
        connection.apply opts
        transport.set_server if server
        transport.apply opts
      end

      attr_reader :handler

      # @return [Connection]
      def connection()
        @connection ||= Connection.wrap(Cproton.pni_connection_driver_connection(@impl))
      end

      # @return [Transport]
      def transport()
        @transport ||= Transport.wrap(Cproton.pni_connection_driver_transport(@impl))
      end

      # @return [IO] Allows ConnectionDriver to be passed directly to {IO#select}
      def to_io() @io; end

      # @return [Bool] True if the driver can read more data
      def can_read?() Cproton.pni_connection_driver_read_size(@impl) > 0; end

      # @return [Bool] True if the driver has data to write
      def can_write?() Cproton.pni_connection_driver_write_size(@impl) > 0; end

      # True if the ConnectionDriver has nothing left to do: both sides of the
      # transport are closed and there are no events to dispatch.
      def finished?() Cproton.pn_connection_driver_finished(@impl); end

      # Dispatch available events, call the relevant on_* methods on the {#handler}.
      def dispatch(extra_handlers = nil)
        extra_handlers ||= []
        while event = Event::Event.wrap(Cproton.pn_connection_driver_next_event(@impl))
          pre_dispatch(event)
          event.dispatch(@handler)
          extra_handlers.each { |h| event.dispatch h }
        end
      end

      # Read from IO without blocking.
      # IO errors are not raised, they are passed to {#handler}.on_transport_error by {#dispatch}
      def read
        size = Cproton.pni_connection_driver_read_size(@impl)
        return if size <= 0
        @io.read_nonblock(size, @rbuf) # Use the same string rbuf for reading each time
        Cproton.pni_connection_driver_read_copy(@impl, @rbuf) unless @rbuf.empty?
      rescue Errno::EWOULDBLOCK, Errno::EAGAIN, Errno::EINTR
        # Try again later.
      rescue EOFError         # EOF is not an error
        close_read
      rescue IOError, SystemCallError => e     #  is passed to the transport
        close e
      end

      # Write to IO without blocking.
      # IO errors are not raised, they are passed to {#handler}.on_transport_error by {#dispatch}
      def write
        n = @io.write_nonblock(Cproton.pn_connection_driver_write_buffer(@impl))
        Cproton.pn_connection_driver_write_done(@impl, n) if n > 0
      rescue Errno::EWOULDBLOCK, Errno::EAGAIN, Errno::EINTR
        # Try again later.
      rescue IOError, SystemCallError => e
        close e
      end

      # Generate timed events and IO, for example idle-timeout and heart-beat events.
      # May generate events for {#dispatch} and change the readable/writeable state.
      #
      # @param [Time] now the current time, defaults to {Time#now}.
      #
      # @return [Time] time of the next scheduled event, or nil if there are no
      # scheduled events. If non-nil, tick() must be called again no later than
      # this time.
      def tick(now=Time.now)
        transport = Cproton.pni_connection_driver_transport(@impl)
        ms = Cproton.pn_transport_tick(transport, (now.to_r * 1000).to_i)
        return ms.zero? ? nil : Time.at(ms.to_r / 1000);
      end

      # Do read, tick, write and dispatch without blocking.
      # @param [Bool] io_readable true if the IO might be readable
      # @param [Bool] io_writable true if the IO might be writeable
      # @param [Time] now the current time
      # @return [Time] Latest time to call {#process} again for scheduled events,
      # or nil if there are no scheduled events
      def process(io_readable=true, io_writable=true, now=Time.now)
        read if io_readable
        next_tick = tick(now)
        if io_writable
          dispatch
          write
        end
        dispatch
        return next_tick
      end

      # Close the read side of the transport
      def close_read
        return if Cproton.pn_connection_driver_read_closed(@impl)
        Cproton.pn_connection_driver_read_close(@impl)
        @io.close_read
      end

      # Close the write side of the transport
      def close_write
        return if Cproton.pn_connection_driver_write_closed(@impl)
        Cproton.pn_connection_driver_write_close(@impl)
        @io.close_write
      end

      # Close both sides of the IO with optional error
      # @param error [Condition] If non-nil pass to {#handler}.on_transport_error on next {#dispatch}
      # Note `error` can be any value accepted by [Condition##make]
      def close(error=nil)
        if error
          cond = Condition.make(error, "proton:io")
          Cproton.pn_connection_driver_errorf(@impl, cond.name, "%s", cond.description)
        end
        close_read
        close_write
      end

      protected

      # Override in subclass to add event context
      def pre_dispatch(event) event; end

    end
  end
end
