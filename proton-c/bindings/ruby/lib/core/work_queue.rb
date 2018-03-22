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

  # A queue of work items to be executed, possibly in a different thread.
  class WorkQueue

    # Add code to be executed by the WorkQueue immediately.
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

    # Schedule work to be executed by the WorkQueue after a delay.
    # Note that tasks scheduled after the WorkQueue closes will be silently dropped
    #
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
