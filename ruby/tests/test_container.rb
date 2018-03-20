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

# MessagingHandler that raises in on_error to catch unexpected errors
class ExceptionMessagingHandler
  def on_error(e) raise e; end
end

class ContainerTest < MiniTest::Test
  include Qpid::Proton

  def test_simple()
    send_handler = Class.new(ExceptionMessagingHandler) do
      attr_reader :accepted, :sent

      def initialize() @ready = Queue.new; end

      def on_sendable(sender)
        sender.send Message.new("foo") unless @sent
        @sent = true
      end

      def on_tracker_accept(tracker)
        @accepted = true
        tracker.connection.close
        @ready << nil
      end

      def wait() @ready.pop(); end
    end.new

    receive_handler = Class.new(ExceptionMessagingHandler) do
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

    c = ServerContainer.new(__method__, {:handler => receive_handler})
    c.connect(c.url, {:handler => send_handler}).open_sender({:name => "testlink"})
    send_handler.wait
    c.wait

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

  # Verify that connection options are sent to the peer
  def test_connection_options
    # Note: user, password and sasl_xxx options are tested by ContainerSASLTest below
    server_handler = Class.new(ExceptionMessagingHandler) do
      def initialize() @connection = Queue.new; end
      def on_connection_open(c)
        @connection << c
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
    # Transport options set by listener, by Connection#open it is too late
    cont = ServerContainer.new(__method__, {
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
    server = server_handler.connection.pop
    cont.wait

    c = server
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

  # Test for time out on connecting to an unresponsive server
  def test_idle_timeout_server_no_open
    s = TCPServer.new(0)
    cont = Container.new(__method__)
    cont.connect(":#{s.addr[1]}", {:idle_timeout => 0.1, :handler => ExceptionMessagingHandler.new })
    ex = assert_raises(Qpid::Proton::Condition) { cont.run }
    assert_match(/resource-limit-exceeded/, ex.to_s)
  ensure
    s.close if s
  end

  # Test for time out on unresponsive client
  def test_idle_timeout_client
    server = ServerContainer.new("#{__method__}.server", {:idle_timeout => 0.1})
    client_handler = Class.new(ExceptionMessagingHandler) do
      def initialize() @ready, @block = Queue.new, Queue.new; end
      attr_reader :ready, :block
      def on_connection_open(c)
        @ready.push nil        # Tell the main thread we are now open
        @block.pop             # Block the client so the server will time it out
      end
    end.new
    client = Container.new(nil, "#{__method__}.client")
    client.connect(server.url, {:handler => client_handler})
    client_thread = Thread.new { client.run }
    client_handler.ready.pop    # Wait till the client has connected
    server.wait                 # Exits when the connection closes from idle-timeout
    client_handler.block.push nil   # Unblock the client
    ex = assert_raises(Qpid::Proton::Condition) { client_thread.join }
    assert_match(/resource-limit-exceeded/, ex.to_s)
  end

  # Make sure we stop and clean up if an aborted connection causes a handler to raise.
  # https://issues.apache.org/jira/browse/PROTON-1791
  def test_handler_raise
    cont = ServerContainer.new(__method__)
    client_handler = Class.new(MessagingHandler) do
      # TestException is < Exception so not handled by default rescue clause
      def on_connection_open(c) raise TestException.new("Bad Dog"); end
    end.new
    threads = 3.times.collect { Thread.new { cont.run } }
    sleep 0.01 while cont.running < 3 # Wait for all threads to be running
    sockets = 2.times.collect { TCPSocket.new("", cont.port) }
    cont.connect_io(sockets[1]) # No exception
    cont.connect_io(sockets[0], {:handler => client_handler}) # Should stop container

    threads.each { |t| assert_equal("Bad Dog", assert_raises(TestException) {t.join}.message) }
    sockets.each { |s| assert s.closed? }
    assert cont.listener.to_io.closed?
    assert_raises(Container::StoppedError) { cont.run }
    assert_raises(Container::StoppedError) { cont.listen "" }
  end
end

