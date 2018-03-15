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
require 'socket'

# Container that listens on a random port
class TestContainer < Qpid::Proton::Container

  def initialize(handler, lopts=nil, id=nil)
    super handler, id
    @listener = listen_io(TCPServer.open(0), ListenOnceHandler.new(lopts))
  end

  def port() @listener.to_io.addr[1]; end
  def url() "amqp://:#{port}"; end#
end

class ContainerTest < MiniTest::Test
  include Qpid::Proton

  def test_simple()
    send_handler = Class.new(MessagingHandler) do
      attr_reader :accepted, :sent
      def on_sendable(sender)
        sender.send Message.new("foo") unless @sent
        @sent = true
      end

      def on_tracker_accept(tracker)
        @accepted = true
        tracker.connection.close
      end
    end.new

    receive_handler = Class.new(MessagingHandler) do
      attr_reader :message, :link
      def on_receiver_open(link)
        @link = link
        @link.open
        @link.flow(1)
      end

      def on_message(delivery, message)
        @message = message;
        delivery.update Disposition::ACCEPTED
        delivery.settle
      end
    end.new

    c = TestContainer.new(receive_handler, {}, __method__)
    c.connect(c.url, {:handler => send_handler}).open_sender({:name => "testlink"})
    c.run

    assert send_handler.accepted
    assert_equal "testlink", receive_handler.link.name
    assert_equal "foo", receive_handler.message.body
    assert_equal "test_simple", receive_handler.link.connection.container_id
  end

  class CloseOnOpenHandler < TestHandler
    def on_connection_open(c) super; c.close; end
  end

  def test_auto_stop_one
    # A listener and a connection
    start_stop_handler = Class.new do
      def on_container_start(c) @start = c; end
      def on_container_stop(c) @stop = c; end
      attr_reader :start, :stop
    end.new
    c = Container.new(start_stop_handler, __method__)
    threads = 3.times.collect { Thread.new { c.run } }
    sleep(0.01) while c.running < 3
    assert_equal c, start_stop_handler.start
    l = c.listen_io(TCPServer.new(0), ListenOnceHandler.new({ :handler => CloseOnOpenHandler.new}))
    c.connect("amqp://:#{l.to_io.addr[1]}", { :handler => CloseOnOpenHandler.new} )
    threads.each { |t| assert t.join(1) }
    assert_equal c, start_stop_handler.stop
    assert_raises(Container::StoppedError) { c.run }
  end

  def test_auto_stop_two
    # Connect between different containers
    c1, c2 = Container.new("#{__method__}-1"), Container.new("#{__method__}-2")
    threads = [ Thread.new {c1.run }, Thread.new {c2.run } ]
    l = c2.listen_io(TCPServer.new(0), ListenOnceHandler.new({ :handler => CloseOnOpenHandler.new}))
    c1.connect(l.url, { :handler => CloseOnOpenHandler.new} )
    assert threads.each { |t| t.join(1) }
    assert_raises(Container::StoppedError) { c1.run }
    assert_raises(Container::StoppedError) { c2.connect("") }
  end

  def test_auto_stop_listener_only
    c = Container.new(__method__)
    # Listener only, external close
    t = Thread.new { c.run }
    l = c.listen_io(TCPServer.new(0))
    l.close
    assert t.join(1)
  end

  def test_stop_empty
    c = Container.new(__method__)
    threads = 3.times.collect { Thread.new { c.run } }
    sleep(0.01) while c.running < 3
    assert_nil threads[0].join(0.001) # Not stopped
    c.stop
    assert c.stopped
    assert_raises(Container::StoppedError) { c.connect("") }
    assert_raises(Container::StoppedError) { c.run }
    threads.each { |t| assert t.join(1) }
  end

  def test_stop
    c = Container.new(__method__)
    c.auto_stop = false

    l = c.listen_io(TCPServer.new(0))
    threads = 3.times.collect { Thread.new { c.run } }
    sleep(0.01) while c.running < 3
    l.close
    assert_nil threads[0].join(0.001) # Not stopped, no auto_stop

    l = c.listen_io(TCPServer.new(0)) # New listener
    conn = c.connect("amqp://:#{l.to_io.addr[1]}")
    c.stop
    assert c.stopped
    threads.each { |t| assert t.join(1) }

    assert_raises(Container::StoppedError) { c.run }
    assert_equal 0, c.running
    assert_nil l.condition
    assert_nil conn.condition
  end

  def test_bad_host
    cont = Container.new(__method__)
    assert_raises (SocketError) { cont.listen("badlisten.example.com:999") }
    assert_raises (SocketError) { c = cont.connect("badconnect.example.com:999") }
  end

  # Verify that connection options are sent to the peer and available as Connection methods
  def test_connection_options
    # Note: user, password and sasl_xxx options are tested by ContainerSASLTest below
    server_handler = Class.new(MessagingHandler) do
      def on_error(e) raise e.inspect; end
      def on_connection_open(c)
        @connection = c
        c.open({
                :virtual_host => "server.to.client",
                :properties => { :server => :client },
                :offered_capabilities => [ :s1 ],
                :desired_capabilities => [ :s2 ],
                :container_id => "box",
               })
        c.close
      end
      attr_reader :connection
    end.new
    # Transport options must be provided to the listener, by Connection#open it is too late
    cont = TestContainer.new(nil, {
                                   :handler => server_handler,
                                   :idle_timeout => 88,
                                   :max_sessions =>1000,
                                   :max_frame_size => 8888,
                                  })
    client = cont.connect(cont.url,
                          {:virtual_host => "client.to.server",
                           :properties => { :foo => :bar, "str" => "str" },
                           :offered_capabilities => [:c1 ],
                           :desired_capabilities => ["c2" ],
                           :idle_timeout => 42,
                           :max_sessions =>100,
                           :max_frame_size => 4096,
                           :container_id => "bowl"
                          })
    cont.run
    c = server_handler.connection
    assert_equal "client.to.server", c.virtual_host
    assert_equal({ :foo => :bar, :str => "str" }, c.properties)
    assert_equal([:c1], c.offered_capabilities)
    assert_equal([:c2], c.desired_capabilities)
    assert_equal 21, c.idle_timeout # Proton divides by 2
    assert_equal 100, c.max_sessions
    assert_equal 4096, c.max_frame_size
    assert_equal "bowl", c.container_id

    c = client
    assert_equal "server.to.client", c.virtual_host
    assert_equal({ :server => :client }, c.properties)
    assert_equal([:s1], c.offered_capabilities)
    assert_equal([:s2], c.desired_capabilities)
    assert_equal "box", c.container_id
    assert_equal 8888, c.max_frame_size
    assert_equal 44, c.idle_timeout # Proton divides by 2
    assert_equal 100, c.max_sessions
  end
end


