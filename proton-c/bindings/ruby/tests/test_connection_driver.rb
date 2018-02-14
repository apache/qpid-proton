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
# distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.


require 'test_tools'
require 'minitest/unit'

include Qpid::Proton

# Test delivery of raw proton events

class RawDriverTest < MiniTest::Test

  # Raw handler to record all on_xxx calls
  class RecordingHandler
    def initialize() @calls =[]; end
    def proton_adapter_class() nil; end # Raw adapter
    attr_reader :calls

    def method_missing(name, *args) respond_to_missing?(name) ? @calls << name : super; end
    def respond_to_missing?(name, private=false); (/^on_/ =~ name); end
    def respond_to?(name, all=false) super || respond_to_missing?(name); end # For ruby < 1.9.2
  end

  def test_send_recv
    send_class = Class.new do
      def proton_adapter_class() nil; end # Raw adapter
      attr_reader :outcome
      def on_link_flow(event) event.sender.send Message.new("foo"); end
      def on_delivery(event)
        @outcome = event.delivery.state
        event.connection.close;
      end
    end

    recv_class = Class.new do
      def proton_adapter_class() nil; end # Raw adapter
      attr_reader :message
      def on_connection_remote_open(event) event.context.open; end
      def on_session_remote_open(event) event.context.open; end
      def on_link_remote_open(event) event.link.open; event.link.flow(1); end
      def on_delivery(event) @message = event.message; event.delivery.accept; end
    end

    d = DriverPair.new(send_class.new, recv_class.new)
    d.client.connection.open(:container_id => __method__);
    d.client.connection.open_sender()
    d.run
    assert_equal(d.server.handler.message.body, "foo")
    assert_equal Transfer::ACCEPTED, d.client.handler.outcome
  end

  def test_idle

    d = DriverPair.new(RecordingHandler.new, RecordingHandler.new)
    ms = 444
    secs = Rational(ms, 1000)   # Use rationals to keep it accurate
    opts = {:idle_timeout => secs}
    d.client.transport.apply(opts)
    assert_equal(ms, d.client.transport.idle_timeout) # Transport converts to ms
    d.server.transport.set_server
    d.client.connection.open(opts)

    start = Time.at(1)          # Dummy timeline
    tick = d.run start          # Process all IO events
    assert_equal(secs/4, tick - start)
    assert_equal [:on_connection_init, :on_connection_local_open, :on_connection_bound], d.client.handler.calls
    assert_equal [:on_connection_init, :on_connection_bound, :on_connection_remote_open, :on_transport], d.server.handler.calls
    assert_equal (ms), d.client.transport.idle_timeout
    assert_equal (ms/2), d.server.transport.remote_idle_timeout # proton changes the value
    assert_equal (secs/2), d.server.connection.idle_timeout

    # Now update the time till we get connections closing
    d.each { |x| x.handler.calls.clear }
    d.run(start + secs - 0.001) # Should nothing, timeout not reached
    assert_equal [[],[]], d.collect { |x| x.handler.calls }
    d.run(start + secs*2)   # After 2x timeout, connections should close
    assert_equal [[:on_transport_error, :on_transport_tail_closed, :on_transport_head_closed, :on_transport_closed], [:on_connection_remote_close, :on_transport_tail_closed, :on_transport_head_closed, :on_transport_closed]], d.collect { |x| x.handler.calls }
  end

  # Test each_session/each_link methods both with a block and returning Enumerator
  def test_enumerators
    connection = Connection.new()
    (3.times.collect { connection.open_session }).each { |s|
      s.open_sender; s.open_receiver
    }

    assert_equal 3, connection.each_session.to_a.size
    assert_equal 6, connection.each_link.to_a.size

    # Build Session => Set<Links> map using connection link enumerator
    map1 = {}
    connection.each_link { |l| map1[l.session] ||= Set.new; map1[l.session] << l }
    assert_equal 3, map1.size
    map1.each do |session,links|
      assert_equal 2, links.size
      links.each { |l| assert_equal session, l.session }
    end

    # Build Session => Set<Links> map using connection and session blocks
    map2 = {}
    connection.each_session do |session|
      map2[session] = Set.new
      session.each_link { |l| map2[session] << l }
    end
    assert_equal map1, map2

    # Build Session => Set<Links> map using connection session and session enumerators
    map3 = Hash[connection.each_session.collect { |s| [s, Set.new(s.each_link)] }]
    assert_equal map1, map3

    assert_equal [true, true, true], connection.each_sender.collect { |l| l.is_a? Sender }
    assert_equal [true, true, true], connection.each_receiver.collect { |l| l.is_a? Receiver }
    connection.each_session { |session|
      assert_equal [true], session.each_sender.collect { |l| l.is_a? Sender }
      assert_equal [true], session.each_receiver.collect { |l| l.is_a? Receiver }
    }


  end
end
