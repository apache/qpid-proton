#--
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
#++

require 'test_tools'

include Qpid::Proton

class ConnectionDriverTest < Minitest::Test

  def setup
    @sockets = Socket.pair(:LOCAL, :STREAM, 0)
  end

  def test_send_recv
    send_class = Class.new(MessagingHandler) do
      attr_reader :accepted
      def on_sendable(event) event.sender.send Message.new("foo"); end
      def on_accepted(event) event.connection.close; @accepted = true; end
    end

    recv_class = Class.new(MessagingHandler) do
      attr_reader :message
      def on_link_opened(event) event.link.flow(1); event.link.open; end
      def on_message(event) @message = event.message; event.connection.close; end
    end

    sender = ConnectionDriver.new(@sockets[0], send_class.new)
    sender.connection.open();
    sender.connection.open_sender()

    receiver = ConnectionDriver.new(@sockets[1], recv_class.new)
    drivers = [sender, receiver]
    until drivers.all? { |d| d.finished? }
      rd = drivers.select {|d| d.can_read? }
      wr = drivers.select {|d| d.can_write? }
      rs, ws = IO.select(rd, wr)
      ws.each { |d| d.write; d.dispatch }
      rs.each { |d| d.read; d.dispatch }
    end
    assert_equal(receiver.handler.message.body, "foo")
    assert(sender.handler.accepted)
  end

  def test_idle
    idle_class = Class.new(MessagingHandler) do
      def on_connection_bound(event) event.transport.idle_timeout = 10; end
    end
    drivers = [ConnectionDriver.new(@sockets[0], idle_class.new), ConnectionDriver.new(@sockets[1])]
    drivers[0].connection.open()
    now = Time.now
    drivers.each { |d| d.process(true, true, now) } until drivers[0].connection.open?
    assert_equal(10, drivers[0].transport.idle_timeout)
    assert_in_delta(10, (drivers[0].tick(now) - now)*1000, 1)
  end
end
