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
  # A time-sorted list of objects. Thread unsafe.
  class Schedule
    include TimeCompare
    Entry = Struct.new(:time, :item)

    def initialize() @entries = []; end

    def empty?() @entries.empty?; end

    def next_tick()
      @entries.first.time unless @entries.empty?
    end

    # @param at [Time] Insert item at time +at+
    # @param at [Numeric] Insert item at +Time.now \+ at+
    # @param at [0] Insert item at Time.at(0)
    def insert(at, item)
      time = case at
             when 0 then Time.at(0) # Avoid call to Time.now for immediate tasks
             when Numeric then Time.now + at
             else at
             end
      index = time && ((0...@entries.size).bsearch { |i| @entries[i].time > time })
      @entries.insert(index || -1, Entry.new(time, item))
    end

    # Return next item due at or before time, else nil
    def pop(time)
      @entries.shift.item if !@entries.empty? && before_eq(@entries.first.time, time)
    end

    def clear() @entries.clear; end
  end
end
