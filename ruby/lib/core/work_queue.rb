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

  # A thread-safe queue of work for multi-threaded programs.
  #
  # A {Container} can have multiple threads calling {Container#run}
  # The container ensures that work associated with a single {Connection} or
  # {Listener} is _serialized_ - two threads will never concurrently call
  # handlers associated with the same object.
  #
  # To have your own code serialized in the same, add a block to the connection's
  # {WorkQueue}. The block will be invoked as soon as it is safe to do so.
  #
  # A {Connection} and the objects associated with it ({Session}, {Sender},
  # {Receiver}, {Delivery}, {Tracker}) are not thread safe, so if you have
  # multiple threads calling {Container#run} or if you want to affect objects
  # managed by the container from non-container threads you need to use the
  # {WorkQueue}
  #
  class WorkQueue

    # Error raised if work is added after the queue has been stopped.
    class StoppedError < Qpid::Proton::StoppedError
      def initialize() super("WorkQueue has been stopped"); end
    end

    # Add a block of code to be invoked in sequence.
    #
    # @yield [ ] the block will be invoked with no parameters in the appropriate thread context
    # @note Thread Safe: may be called in any thread.
    # @return [void]
    # @raise [StoppedError] if the queue is closed and cannot accept more work
    def add(&block)
      schedule(0, &block)
    end

    # Schedule a block to be invoked at a certain time.
    #
    # @param at [Time] Invoke block as soon as possible after Time +at+
    # @param at [Numeric] Invoke block after a delay of +at+ seconds from now
    # @yield [ ] (see #add)
    # @note (see #add)
    # @return (see #add)
    # @raise (see #add)
    def schedule(at, &block)
      raise ArgumentError, "no block" unless block_given?
      @lock.synchronize do
        raise @closed if @closed
        @schedule.insert(at, block)
      end
      @container.send :wake
    end

    # @private
    def initialize(container)
      @lock = Mutex.new
      @schedule = Schedule.new
      @container = container
      @closed = nil
    end

    # @private
    def close() @lock.synchronize { @closed = StoppedError.new } end

    # @private
    def process(now)
      while p = @lock.synchronize { @schedule.pop(now) }
        p.call
      end
    end

    # @private
    def next_tick() @lock.synchronize { @schedule.next_tick } end

    # @private
    def empty?() @lock.synchronize { @schedule.empty? } end

    # @private
    def clear() @lock.synchronize { @schedule.clear } end
  end
end
