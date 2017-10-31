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
  # A listener for incoming connections.
  #
  # Create with {Container#listen} or  {Container#listen_with}
  class Listener
    # The listener's container
    attr_reader :container

    # Close the listener
    # @param error [Condition] Optional error condition.
    def close(error=nil)
      @closed ||= Condition.make(error) || true
      @io.close_read rescue nil # Cause listener to wake out of IO.select
    end

    # Get the {IO} server socket used by the listener
    def to_io() @io; end

    private                     # Called by {Container}

    def initialize(io, handler)
      @io, @handler = io, handler
    end

    def process
      unless @closed
        unless @open_dispatched
          dispatch(:on_open)
          @open_dispatched = true
        end
        begin
          return @io.accept, dispatch(:on_accept)
        rescue IO::WaitReadable, Errno::EINTR
        rescue IOError, SystemCallError => e
          close e
        end
      end
      if @closed
        dispatch(:on_error, @closed) if @closed != true
        dispatch(:on_close)
        close @io unless @io.closed? rescue nil
      end
    end

    def can_read?() true; end
    def can_write?() false; end
    def finished?() @closed; end

    # TODO aconway 2017-11-06: logging strategy
    TRUE = Set[:true, :"1", :yes, :on]
    def log?()
      enabled = ENV['PN_TRACE_EVT']
      TRUE.include? enabled.downcase.to_sym if enabled
    end

    def dispatch(method, *args)
      STDERR.puts "(Listener 0x#{object_id.to_s(16)})[#{method}]" if log?
      @handler.send(method, self, *args) if @handler && @handler.respond_to?(method)
    end
  end


  # Class that handles listener events and provides options for accepted
  # connections. This class simply returns a fixed set of options for every
  # connection accepted, but you can subclass and override all of the on_
  # methods to provide more interesting behaviour.
  class ListenHandler
    # @param opts [Hash] Options to return from on_accept.
    def initialize(opts={}) @opts = opts; end

    # Called when the listener is ready to accept connections.
    # @param listener [Listener] The listener
    def on_open(listener) end

    # Called if an error occurs.
    # If there is an error while opening the listener, this method is
    # called and {#on_open} is not
    # @param listener [Listener]
    # @param what [Condition] Information about the error.
    def on_error(listener, what) end

    # Called when a listener accepts a new connection.
    # @param listener [Listener] The listener
    # @return [Hash] Options to apply to the incoming connection, see {#connect}
    def on_accept(listener) @opts; end

    # Called when the listener closes.
    # @param listener [Listener] The listener accepting the connection.
    def on_close(listener) end
  end
end
