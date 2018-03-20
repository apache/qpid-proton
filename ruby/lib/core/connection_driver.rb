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

module Qpid::Proton

  # Associate an AMQP {Connection} and {Transport} with an {IO}
  #
  # - {#read} reads AMQP binary data from the {IO} and generates events
  # - {#tick} generates timing-related events
  # - {#event} gets events to be dispatched to {Handler::MessagingHandler}s
  # - {#write} writes AMQP binary data to the {IO}
  #
  # Thread safety: The {ConnectionDriver} is not thread safe but separate
  # {ConnectionDriver} instances can be processed concurrently. The
  # {Container} handles multiple connections concurrently in multiple threads.
  #
  class ConnectionDriver
    # Create a {Connection} and {Transport} associated with +io+
    # @param io [IO] An {IO} or {IO}-like object that responds
    #   to {IO#read_nonblock} and {IO#write_nonblock}
    def initialize(io)
      @impl = Cproton.pni_connection_driver or raise NoMemoryError
      @io = io
      @rbuf = ""              # String for re-usable read buffer
    end

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

    # Get the next event to dispatch, nil if no events available
    def event()
      e = Cproton.pn_connection_driver_next_event(@impl)
      Event.new(e) if e
    end

    # True if {#event} will return non-nil
    def event?() Cproton.pn_connection_driver_has_event(@impl); end

    # Iterator for all available events
    def each_event()
      while e = event
        yield e
      end
    end

    # Non-blocking read from {#io}, generate events for {#event}
    # IO errors are returned as transport errors by {#event}, not raised
    def read
      size = Cproton.pni_connection_driver_read_size(@impl)
      return if size <= 0
      @io.read_nonblock(size, @rbuf) # Use the same string rbuf for reading each time
      Cproton.pni_connection_driver_read_copy(@impl, @rbuf) unless @rbuf.empty?
    rescue Errno::EWOULDBLOCK, Errno::EAGAIN, Errno::EINTR
      # Try again later.
    rescue EOFError         # EOF is not an error
      close_read
    rescue IOError, SystemCallError => e
      close e
    end

    # Non-blocking write to {#io}
    # IO errors are returned as transport errors by {#event}, not raised
    def write
      data = Cproton.pn_connection_driver_write_buffer(@impl)
      return unless data && data.size > 0
      n = @io.write_nonblock(data)
      Cproton.pn_connection_driver_write_done(@impl, n) if n > 0
    rescue Errno::EWOULDBLOCK, Errno::EAGAIN, Errno::EINTR
      # Try again later.
    rescue IOError, SystemCallError => e
      close e
    end

    # Handle time-related work, for example idle-timeout events.
    # May generate events for {#event} and change {#can_read?}, {#can_write?}
    #
    # @param [Time] now the current time, defaults to {Time#now}.
    #
    # @return [Time] time of the next scheduled event, or nil if there are no
    # scheduled events. If non-nil you must call {#tick} again no later than
    # this time.
    def tick(now=Time.now)
      transport = Cproton.pni_connection_driver_transport(@impl)
      ms = Cproton.pn_transport_tick(transport, (now.to_r * 1000).to_i)
      @next_tick = ms.zero? ? nil : Time.at(ms.to_r / 1000);
      unless @next_tick
        idle = Cproton.pn_transport_get_idle_timeout(transport);
        @next_tick = now + (idle.to_r / 1000) unless idle.zero?
      end
      @next_tick
    end

    # Time returned by the last call to {#tick}
    attr_accessor :next_tick

    # Disconnect the write side of the transport, *without* sending an AMQP
    # close frame. To close politely, you should use {Connection#close}, the
    # transport will close itself once the protocol close is complete.
    #
    def close_write error=nil
      set_error error
      Cproton.pn_connection_driver_write_close(@impl)
      @io.close_write rescue nil # Allow double-close
    end

    # Is the read side of the driver closed?
    def read_closed?() Cproton.pn_connection_driver_read_closed(@impl); end

    # Is the write side of the driver closed?
    def write_closed?() Cproton.pn_connection_driver_read_closed(@impl); end

    # Disconnect the read side of the transport, without waiting for an AMQP
    # close frame. See comments on {#close_write}
    def close_read error=nil
      set_error error
      Cproton.pn_connection_driver_read_close(@impl)
      @io.close_read rescue nil # Allow double-close
    end

    # Disconnect both sides of the transport sending/waiting for AMQP close
    # frames. See comments on {#close_write}
    def close error=nil
      close_write error
      close_read
    end

    private

    def set_error err
      transport.condition ||= Condition.convert(err, "proton:io") if err
    end
  end

  # A {ConnectionDriver} that feeds raw proton events to a handler.
  class HandlerDriver < ConnectionDriver
    # Combine an {IO} with a handler and provide
    # a simplified way to run the driver via {#process}
    #
    # @param io [IO]
    # @param handler [Handler::MessagingHandler] to receive raw events in  {#dispatch} and {#process}
    def initialize(io, handler)
      super(io)
      @handler = handler
      @adapter = Handler::Adapter.adapt(handler)
    end

    # @return [MessagingHandler] The handler dispatched to by {#process}
    attr_reader :handler

    # Dispatch all available raw proton events from {#event} to {#handler}
    def dispatch()
      each_event do |e|
        case e.method           # Events that affect the driver
        when :on_transport_tail_closed then close_read
        when :on_transport_head_closed then close_write
        when :on_transport_closed then @io.close rescue nil # Allow double-close
        end
        e.dispatch @adapter
      end
    end

    # Do {#read}, {#tick}, {#write} and {#dispatch} without blocking.
    # @param [Time] now the current time
    # @return [Time] Latest time to call {#process} again for scheduled events,
    #   or nil if there are no scheduled events
    def process(now=Time.now)
      read
      dispatch
      next_tick = tick(now)
      dispatch
      write
      dispatch
      return next_tick
    end
  end
end
