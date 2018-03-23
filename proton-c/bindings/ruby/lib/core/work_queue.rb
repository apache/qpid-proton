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
  # Instances of {Connection} and objects associated with it ({Session}, {Sender},
  # {Receiver}, {Delivery}, {Tracker}) are not thread-safe and must be
  # used correctly when multiple threads call {Container#run}
  #
  # Calls to {MessagingHandler} methods by the {Container} are automatically
  # serialized for each connection instance. Other threads may have code
  # similarly serialized by adding it to the {Connection#work_queue} for the
  # connection.  Each object related to a {Connection} also provides a
  # +work_queue+ method.
  #
  class WorkQueue

    # Add code to be executed in series with other {Container} operations on the
    # work queue's owner. The code will be executed as soon as possible.
    #
    # @note Thread Safe: may be called in any thread.
    # @param non_block [Boolean] if true raise {ThreadError} if the operation would block.
    # @yield [ ] the block will be invoked with no parameters in the {WorkQueue} context,
    #  which may be a different thread.
    # @return [void]
    # @raise [ThreadError] if +non_block+ is true and the operation would block
    # @raise [EOFError] if the queue is closed and cannot accept more work
    def add(non_block=false, &block)
      @schedule.add(Time.at(0), non_block, &block)
      @container.send :wake
    end

    # Schedule code to be executed after +delay+ seconds in series with other
    # {Container} operations on the work queue's owner.
    #
    # Work scheduled for after the {WorkQueue} has closed will be silently dropped.
    #
    # @note (see #add)
    # @param delay delay in seconds until the block is added to the queue.
    # @param (see #add)
    # @yield (see #add)
    # @return [void]
    # @raise (see #add)
    def schedule(delay, non_block=false, &block)
      @schedule.add(Time.now + delay, non_block, &block)
      @container.send :wake
    end

    private

    def initialize(container)
      @schedule = Schedule.new
      @container = container
    end

    def close() @schedule.close; end
    def process(now) @schedule.process(now); end
    def next_tick() @schedule.next_tick; end
  end
end
