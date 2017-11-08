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

require 'thread'
require 'set'
require_relative 'listener'

module Qpid::Proton
  # An AMQP container manages a set of {Connection}s which contain {#Sender} and
  # {#Receiver} links to transfer messages.
  #
  # TODO aconway 2017-10-26: documentthreading/dispatch role
  #
  # Usually, each AMQP client or server process has a single container for
  # all of its connections and links.
  class Container
    private

    def amqp_uri(s) Qpid::Proton::amqp_uri s; end

    class ConnectionDriver < Qpid::Proton::ConnectionDriver
      def initialize container, io, opts, server=false
        super io, opts, server
        @container = container
      end

      def final() end
      def pre_dispatch(event) event.container = @container; end
    end

    public

    # Create a new Container
    #
    # @param handler [MessagingHandler] Optional default handler for connections
    #   that do not have their own handler (see {#connect} and {#listen})
    #
    #   @note For multi-threaded code, it is recommended to use a separate
    #   handler instance for every connection, as a shared global handler can be
    #   called concurrently for every connection that uses it.
    # @param id [String] A unique ID for this container. Defaults to a random UUID.
    #
    def initialize(handler = nil, id = nil)
      # Allow ID as sole argument
      (handler, id = nil, handler.to_str) if (id.nil? && handler.respond_to?(:to_str))
      raise TypeError, "Expected MessagingHandler, got #{handler.class}" if handler && !handler.is_a?(Qpid::Proton::Handler::MessagingHandler)

      # TODO aconway 2017-11-08: allow handlers, opts for backwards compat?
      @handler = handler
      @id = (id || SecureRandom.uuid).freeze
      @work = Queue.new
      @work.push self           # Let the first #run thread select
      @wake = IO.pipe
      @dummy = ""               # Dummy buffer for draining wake pipe
      @lock = Mutex.new
      @selectables = Set.new    # ConnectionDrivers and Listeners
      @auto_stop = true
      @active = 0               # activity (connection, listener) counter for auto_stop
      @running = 0              # concurrent calls to #run
    end

    # @return [String] Unique identifier for this container
    attr_reader :id

    # Open an AMQP connection.
    #
    # @param url [String, URI] Open a {TCPSocket} to url.host, url.port.
    # url.scheme must be "amqp" or "amqps", url.scheme.nil? is treated as "amqp"
    # url.user, url.password are used as defaults if opts[:user], opts[:password] are nil
    # @option (see Connection#open)
    # @return [Connection] The new AMQP connection
    #
    def connect(url, opts = {})
      url = amqp_uri(url)
      opts[:user] ||= url.user
      opts[:password] ||= url.password
      # TODO aconway 2017-10-26: Use SSL for amqps URLs
      connect_io(TCPSocket.new(url.host, url.port), opts)
    end

    # Open an AMQP protocol connection on an existing {IO} object
    # @param io [IO] An existing {IO} object, e.g. a {TCPSocket}
    # @option (see Connection#open)
    def connect_io(io, opts = {})
      cd = connection_driver(io, opts)
      cd.connection.open()
      add(cd).connection
    end

    # Listen for incoming AMQP connections
    #
    # @param url [String,URI] Listen on host:port of the AMQP URL
    # @param handler [ListenHandler] A {ListenHandler} object that will be called
    # with events for this listener and can generate a new set of options for each one.
    # @return [Listener] The AMQP listener.
    def listen(url, handler=ListenHandler.new)
      url = amqp_uri(url)
      # TODO aconway 2017-11-01: amqps
      listen_io(TCPServer.new(url.host, url.port), handler)
    end

    # Listen for incoming AMQP connections on an existing server socket.
    # @param io A server socket, for example a {TCPServer}
    # @param handler [ListenHandler] Handler for events from this listener
    def listen_io(io, handler=ListenHandler.new)
      add(Listener.new(io, handler))
    end

    # Run the container: wait for IO activity, dispatch events to handlers.
    #
    # More than one thread can call {#run} concurrently, the container will use
    # all the {#run} ,threads as a pool to handle multiple connections
    # concurrently.  The container ensures that handler methods for a single
    # connection (or listener) instance are serialized, even if the container
    # has multiple threads.
    #
    def run()
      @lock.synchronize { @running += 1 }

      unless @on_start
        @on_start = true
        # TODO aconway 2017-10-28: proper synthesized event for on_start
        event = Class.new do
          def initialize(c) @container = c; end
          attr_reader :container
        end.new(self)
        @handler.on_start(event) if @handler && @handler.respond_to?(:on_start)
      end

      while x = @work.pop
        case x
        when Container then
          # Only one thread can select at a time
          r, w = [@wake[0]], []
          @lock.synchronize do
            @selectables.each do |s|
              r << s if s.send :can_read?
              w << s if s.send :can_write?
            end
          end
          r, w = IO.select(r, w)
          selected = Set.new(r).merge(w)
          if selected.delete?(@wake[0]) # Drain the wake pipe
            begin
              @wake[0].read_nonblock(256, @dummy) while true
            rescue Errno::EWOULDBLOCK, Errno::EAGAIN, Errno::EINTR
            end
          end
          @lock.synchronize do
            if @stop_all
              selected = @selectables
              @selectables = Set.new
              selected.each { |s| s.close }
              @work << nil if selected.empty? # Already idle, initiate stop now
              @stop_all = false
            else
              @selectables.subtract(selected)
            end
          end
          # Move selected items to the work queue for serialized processing.
          @lock.synchronize { @selectables.subtract(selected) }
          selected.each { |s| @work << s } # Queue up all the work
          @work << self                    # Allow another thread to select()
        when ConnectionDriver then
          x.process
          rearm x
        when Listener then
          io, opts = x.send :process
          add(connection_driver(io, opts, true)) if io
          rearm x
        end
        # TODO aconway 2017-10-26: scheduled tasks
      end
    ensure
      @running -= 1
      if @running > 0 # Signal the next #run thread that we are stopping
        @work << nil
        wake
      end
    end

    # @!attribute auto_stop [rw]
    #   @return [Bool] With auto_stop enabled, all calls to {#run} will return when the
    #   container's last activity (connection, listener or scheduled event) is
    #   closed/completed. With auto_stop disabled {#run} does not return.
    def auto_stop=(enabled) @lock.synchronize { @auto_stop=enabled }; wake; end
    def auto_stop() @lock.synchronize { @auto_stop }; end

    # Enable {#auto_stop} and close all connections and listeners with error.
    # {#stop} returns immediately, calls to {#run} will return when all activity is finished.
    # @param error [Condition] If non-nil pass to {#handler}.on_error
    # Note `error` can be any value accepted by [Condition##make]
    def stop(error=nil)
      @lock.synchronize do
        @auto_stop = true
        @stop_all = true
        wake
      end
    end

    private

    # Always wake when we add new work
    def work(s) work << s; wake; end

    def wake()
      @wake[1].write_nonblock('x') rescue nil
    end

    def connection_driver(io, opts, server=false)
      opts[:container_id] ||= @id
      opts[:handler] ||= @handler
      ConnectionDriver.new(self, io, opts, server)
    end

    def add(s)
      @lock.synchronize do
        @active += 1
      end
      @work << s
      wake
      return s
    end

    def rearm s
      if s.send :finished?
        @lock.synchronize { @work << nil if (@active -= 1).zero? && @auto_stop }
      else
        @lock.synchronize { @selectables << s }
      end
      wake
    end
  end
end
