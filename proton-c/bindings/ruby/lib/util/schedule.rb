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

  # @private
  module TimeCompare
    # t1 <= t2, where nil is treated as "distant future"
    def before_eq(t1, t2) (t1 && t2) ? (t1 <= t2) : t1; end

    # min(t1, t2) where nil is treated as "distant future"
    def earliest(t1, t2) before_eq(t1, t2) ? t1 :  t2; end
  end

  # @private
  # A sorted, thread-safe list of scheduled Proc.
  # Note: calls to #process are always serialized, but calls to #add may be concurrent.
  class Schedule
    include TimeCompare
    Item = Struct.new(:time, :proc)

    def initialize()
      @lock = Mutex.new
      @items = []
      @closed = false
    end

    def next_tick()
      @lock.synchronize { @items.first.time unless @items.empty? }
    end

    # @return true if the Schedule was previously empty
    # @raise EOFError if schedule is closed
    # @raise ThreadError if +non_block+ and operation would block
    def add(time, non_block=false, &proc)
      # non_block ignored for now, but we may implement a bounded schedule in future.
      @lock.synchronize do
        raise EOFError if @closed
        if at = (0...@items.size).bsearch { |i| @items[i].time > time }
          @items.insert(at, Item.new(time, proc))
        else
          @items << Item.new(time, proc)
        end
        return @items.size == 1
      end
    end

    # @return true if the Schedule became empty as a result of this call
    def process(now)
      due = []
      empty = @lock.synchronize do
        due << @items.shift while (!@items.empty? && before_eq(@items.first.time, now))
        @items.empty?
      end
      due.each { |i| i.proc.call() }
      return empty && !due.empty?
    end

    # #add raises EOFError after #close.
    # #process can still be called to drain the schedule.
    def close()
      @lock.synchronize { @closed = true }
    end
  end
end
