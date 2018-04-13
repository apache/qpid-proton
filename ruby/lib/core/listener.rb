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
  # A listener for incoming connections.
  #
  # Create with {Container#listen} or  {Container#listen_io}.
  # To control the handler and connection options applied to incoming connections,
  # pass a {ListenerHandler} on creation.
  #
  class Listener

    # Class that handles listener events and provides options for accepted
    # connections. This class simply returns a fixed set of options for every
    # connection accepted, but you can subclass and override all of the on_
    # methods to provide more interesting behaviour.
    #
    # *Note*: If a {Listener} method raises an exception, it will stop the {Container}
    # that the handler is running in. See {Container#run}
    class Handler
      # @param opts [Hash] Options to return from on_accept.
      def initialize(opts=nil) @opts = opts || {}; end

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

    # @return [Container] The listener's container
    attr_reader :container

    # @return [Condition] The error condition if there is one
    attr_reader :condition

    # Initiate closing the the listener.
    # It will not be fully {#closed?} until its {Handler#on_close} is called by the {Container}
    # @param error [Condition] Optional error condition.
    def close(error=nil)
      return if closed? || @closing
      @closing = true
      @condition ||= Condition.convert error
      @io.close_read rescue nil # Force Container IO.select to wake with listener readable.
      nil
    end

    # Get the {IO} server socket used by the listener
    def to_io() @io; end

    # Get the IP port used by the listener
    def port() to_io.addr[1]; end

    # True if the listening socket is fully closed
    def closed?() @io.closed?; end

    private                     # Called by {Container}

    def initialize(io, container)
      @io = io
      @container = container
      @closing = nil
    end
  end
end
