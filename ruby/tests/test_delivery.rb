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
class TestDelivery < MiniTest::Test

  class NoAutoHandler < MessagingHandler
    def on_sender_open(l) l.open({:auto_settle=>false, :auto_accept=>false}); end
    def on_receiver_open(l) l.open({:auto_settle=>false, :auto_accept=>false}); end
  end

  class SendHandler < NoAutoHandler
    def initialize(to_send)
      @unsent = to_send
    end

    def on_connection_open(connection)
      @outcomes = []
      @sender = connection.open_sender("x")
      @unsettled = {}           # Awaiting remote settlement
    end

    attr_reader :outcomes, :unsent, :unsettled

    def on_sendable(sender)
      return if @unsent.empty?
      m = Message.new(@unsent.shift)
      tracker = sender.send(m)
      @unsettled[tracker] = m
    end

    def outcome(method, tracker)
      t = tracker
      m = @unsettled.delete(t)
      @outcomes << [m.body, method, t.id, t.state, t.modifications]
      tracker.connection.close if @unsettled.empty?
    end

    def on_tracker_accept(tracker) outcome(__method__, tracker); end
    def on_tracker_reject(tracker) outcome(__method__, tracker); end
    def on_tracker_release(tracker) outcome(__method__, tracker); end
    def on_tracker_modify(tracker) outcome(__method__, tracker); end
  end

  class ReceiveHandler < NoAutoHandler
    def initialize
      @received = []
    end

    attr_reader :received

    def on_message(delivery, message)
      @received << message.body
      case message.body
      when "accept" then delivery.accept
      when "reject" then delivery.reject
      when "release-really" then delivery.release({:failed=>false}) # AMQP RELEASED
      when "release" then delivery.release # AMQP MODIFIED{ :failed => true }
      when "modify" then delivery.release({:undeliverable => true, :annotations => {:x => 42 }})
      when "modify-empty" then delivery.release({:failed => false, :undeliverable => false, :annotations => {}})
      when "modify-nil" then delivery.release({:failed => false, :undeliverable => false, :annotations => nil})
      when "reject-raise" then raise Reject
      when "release-raise" then raise Release
      else raise inspect
      end
    end
  end

  def test_outcomes
    rh = ReceiveHandler.new
    sh = SendHandler.new(["accept", "reject", "release-really", "release", "modify", "modify-empty", "modify-nil", "reject-raise", "release-raise"])
    c = Container.new(nil, __method__)
    l = c.listen_io(TCPServer.new(0), ListenOnceHandler.new({ :handler => rh }))
    c.connect(l.url, {:handler => sh})
    c.run
    o = sh.outcomes
    assert_equal ["accept", :on_tracker_accept, "1", Transfer::ACCEPTED, nil], o.shift
    assert_equal ["reject", :on_tracker_reject, "2", Transfer::REJECTED, nil], o.shift
    assert_equal ["release-really", :on_tracker_release, "3", Transfer::RELEASED, nil], o.shift
    assert_equal ["release", :on_tracker_modify, "4", Transfer::MODIFIED, {:failed=>true, :undeliverable=>false, :annotations=>nil}], o.shift
    assert_equal ["modify", :on_tracker_modify, "5", Transfer::MODIFIED, {:failed=>true, :undeliverable=>true, :annotations=>{:x => 42}}], o.shift
    assert_equal ["modify-empty", :on_tracker_release, "6", Transfer::RELEASED, nil], o.shift
    assert_equal ["modify-nil", :on_tracker_release, "7", Transfer::RELEASED, nil], o.shift
    assert_equal ["reject-raise", :on_tracker_reject, "8", Transfer::REJECTED, nil], o.shift
    assert_equal ["release-raise", :on_tracker_modify, "9", Transfer::MODIFIED, {:failed=>true, :undeliverable=>false, :annotations=>nil}], o.shift
    assert_empty o
    assert_equal ["accept", "reject", "release-really", "release", "modify", "modify-empty", "modify-nil", "reject-raise", "release-raise"], rh.received
    assert_empty sh.unsettled
  end

  def test_names
    names = ["accepted", "rejected", "released", "modified"]
    states = names.collect { |n| Disposition.const_get(n.upcase) }
    assert_equal names, states.collect { |s| Disposition::name_of(s) }
    assert_equal names, states.collect { |s| Disposition::State::name_of(s) }
    assert_equal names, states.collect { |s| Transfer::name_of(s) }
  end
end
