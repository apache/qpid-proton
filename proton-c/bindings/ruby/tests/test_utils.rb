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

require 'test_tools'
require 'minitest/unit'

class UtilsTest < MiniTest::Test
  include Qpid::Proton

  # Make sure Schedule puts tasks in proper order.
  def test_schedule_empty
    s = Schedule.new
    assert_empty s
    assert_nil s.next_tick
    assert_nil s.pop(nil)
  end

  def test_schedule_insert_pop
    s = Schedule.new
    [3,5,4].each { |i| s.insert(Time.at(i), i) }
    assert_equal Time.at(3), s.next_tick
    assert_nil s.pop(Time.at(2))
    assert_equal [3,4], [s.pop(Time.at(4)), s.pop(Time.at(4))]
    refute_empty s
    assert_nil s.pop(Time.at(4.9))
    assert_equal Time.at(5), s.next_tick
    assert_equal 5, s.pop(Time.at(5))
    assert_empty s
  end

  # Make sure we sort by time and don't change order if same time
  def test_schedule_sort
    s = Schedule.new
    [4.0, 3, 5, 1, 4.1, 4.2 ].each { |n| s.insert(Time.at(n.to_i), n) }
    [1, 3, 4.0, 4.1, 4.2].each { |n| assert_equal n, s.pop(Time.at(4)) }
    refute_empty s              # Not empty but nothing due until time 5
    assert_equal Time.at(5), s.next_tick
    assert_nil s.pop(Time.at(4))
    assert_equal 5, s.pop(Time.at(5))
  end

  def test_schedule_clear
    s = Schedule.new
    [3, 5].each { |n| s.insert(Time.at(n.to_i), n) }
    refute_empty s
    s.clear
    assert_empty s
  end
end
