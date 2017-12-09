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


require 'minitest/autorun'
require 'qpid_proton'
require 'test_tools'
include Qpid::Proton

# Test Delivery and Tracker
class TestDelivery < Minitest::Test

  # Duck-typed handler
  class NoAutoHandler
    @@options = {:auto_settle=>false, :auto_accept=>false}
    def options() @@options; end
  end

  class SendHandler < NoAutoHandler
    def initialize(to_send)
      @unsent = to_send
    end

    def on_connection_opened(event)
      @outcomes = []
      @sender = event.connection.open_sender("x")
      @unsettled = {}           # Awaiting remote settlement
    end

    attr_reader :outcomes, :unsent, :unsettled

    def on_sendable(event)
      return if @unsent.empty?
      m = Message.new(@unsent.shift)
      tracker = event.sender.send(m)
      @unsettled[tracker] = m
    end

    def outcome(event)
      t = event.tracker
      m = @unsettled.delete(t)
      @outcomes << [m.body, event.method, t.id, t.state, t.modifications]
      event.connection.close if @unsettled.empty?
    end

    def on_accepted(event) outcome(event); end
    def on_rejected(event) outcome(event); end
    def on_released(event) outcome(event); end
    def on_modified(event) outcome(event); end
  end

  class ReceiveHandler < NoAutoHandler
    def initialize
      @received = []
    end

    attr_reader :received

    def on_message(event)
      @received << event.message.body
      case event.message.body
      when "accept" then event.delivery.accept
      when "reject" then event.delivery.reject
      when "release-really" then event.delivery.release({:failed=>false}) # AMQP RELEASED
      when "release" then event.delivery.release # AMQP MODIFIED{ :failed => true }
      when "modify" then event.delivery.release({:undeliverable => true, :annotations => {:x => 42 }})
      when "modify-empty" then event.delivery.release({:failed => false, :undeliverable => false, :annotations => {}})
      when "modify-nil" then event.delivery.release({:failed => false, :undeliverable => false, :annotations => nil})
      else raise event.inspect
      end
    end
  end

  def test_outcomes
    rh = ReceiveHandler.new
    sh = SendHandler.new(["accept", "reject", "release-really", "release", "modify", "modify-empty", "modify-nil"])
    c = TestContainer.new(nil, { :handler => rh },  __method__)
    c.connect(c.url, {:handler => sh})
    c.run
    o = sh.outcomes
    assert_equal ["accept", :on_accepted, "1", Transfer::ACCEPTED, nil], o.shift
    assert_equal ["reject", :on_rejected, "2", Transfer::REJECTED, nil], o.shift
    assert_equal ["release-really", :on_released, "3", Transfer::RELEASED, nil], o.shift
    assert_equal ["release", :on_modified, "4", Transfer::MODIFIED, {:failed=>true, :undeliverable=>false, :annotations=>nil}], o.shift
    assert_equal ["modify", :on_modified, "5", Transfer::MODIFIED, {:failed=>true, :undeliverable=>true, :annotations=>{:x => 42}}], o.shift
    assert_equal ["modify-empty", :on_released, "6", Transfer::RELEASED, nil], o.shift
    assert_equal ["modify-nil", :on_released, "7", Transfer::RELEASED, nil], o.shift
    assert_empty o
    assert_equal ["accept", "reject", "release-really", "release", "modify", "modify-empty", "modify-nil"], rh.received
    assert_empty sh.unsettled
  end
end
