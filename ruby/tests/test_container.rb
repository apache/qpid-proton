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

# Since ruby 2.5 the default is true, turn it off since we have tests that deliberately
# leak exceptions from threads to very they are caught properly from Container#run()
(Thread.report_on_exception = false) rescue nil

# MessagingHandler that raises in on_error to catch unexpected errors
class ExceptionMessagingHandler
  def on_error(e) raise e; end
end

class ContainerTest < MiniTest::Test
  include Qpid::Proton

  def test_simple()
    send_handler = Class.new(ExceptionMessagingHandler) do
      attr_reader :accepted, :sent

      def initialize() @sent, @accepted = nil; end

      def on_sendable(sender)
        unless @sent
          m = Message.new("hello")
          m[:foo] = :bar
          sender.send m
        end
        @sent = true
      end

      def on_tracker_accept(tracker)
        @accepted = true
        tracker.connection.close
      end
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
    c.run

    assert send_handler.accepted
    assert_equal "testlink", receive_handler.link.name
    assert_equal "hello", receive_handler.message.body
    assert_equal :bar, receive_handler.message[:foo]
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
    assert_raises (SocketError) { cont.connect("badconnect.example.com:999") }
  end

  # Verify that connection options are sent to the peer
  def test_connection_options
    # Note: user, password and sasl_xxx options are tested by ContainerSASLTest below
    server_handler = Class.new(ExceptionMessagingHandler) do
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
    # Transport options set by listener, by Connection#open it is too late
    cont = ServerContainer.new(__method__, {
      :handler => server_handler,
      :idle_timeout => 88,
      :max_sessions =>1000,
      :max_frame_size => 8888,
    })
    client = cont.connect(cont.url,
      {:virtual_host => "client.to.server",
        :properties => { "foo" => :bar, "str" => "str" },
        :offered_capabilities => [:c1 ],
        :desired_capabilities => [:c2 ],
        :idle_timeout => 42,
        :max_sessions =>100,
        :max_frame_size => 4096,
        :container_id => "bowl"
      })
    cont.run

    c = server_handler.connection
    assert_equal "client.to.server", c.virtual_host
    assert_equal({ "foo" => :bar, "str" => "str" }, c.properties)
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

  def test_link_options
    server_handler = Class.new(ExceptionMessagingHandler) do
      def initialize() @links = []; end
      attr_reader :links
      def on_sender_open(l) @links << l; end
      def on_receiver_open(l) @links << l; end
    end.new

    client_handler = Class.new(ExceptionMessagingHandler) do
      def on_connection_open(c)
        @links = [];
        @links << c.open_sender("s1")
        @links << c.open_sender({:name => "s2-n", :target => "s2-t", :source => "s2-s"})
        @links << c.open_receiver("r1")
        @links << c.open_receiver({:name => "r2-n", :target => "r2-t", :source => "r2-s"})
        c.close
      end
      attr_reader :links
    end.new

    cont = ServerContainer.new(__method__, {:handler => server_handler }, 1)
    cont.connect(cont.url, :handler => client_handler)
    cont.run

    expect = ["test_link_options/1", "s2-n", "test_link_options/2", "r2-n"]
    assert_equal expect, server_handler.links.map(&:name)
    assert_equal expect, client_handler.links.map(&:name)

    expect = [[nil,"s1"], ["s2-s","s2-t"], ["r1",nil], ["r2-s","r2-t"]]
    assert_equal expect, server_handler.links.map { |l| [l.remote_source.address, l.remote_target.address] }
    assert_equal expect, client_handler.links.map { |l| [l.source.address, l.target.address] }
  end

  def extract_terminus_options(t)
    opts = Hash[[:address, :distribution_mode, :durability_mode, :timeout, :expiry_policy].map { |m| [m, t.send(m)] }]
    opts[:filter] = t.filter.map
    opts[:capabilities] = t.capabilities.map
    opts[:dynamic] = t.dynamic?
    opts
  end

  def test_terminus_options
    opts = {
      :distribution_mode => Terminus::DIST_MODE_COPY,
      :durability_mode => Terminus::DELIVERIES,
      :timeout => 5,
      :expiry_policy => Terminus::EXPIRE_WITH_LINK,
      :filter => { :try => 'me' },
      :capabilities => { :cap => 'len' },
    }
    src_opts = { :address => "src", :dynamic => true }.update(opts)
    tgt_opts = { :address => "tgt", :dynamic => false }.update(opts)

    cont = ServerContainer.new(__method__, {}, 1)
    c = cont.connect(cont.url)
    s = c.open_sender({:target => tgt_opts, :source => src_opts })
    assert_equal src_opts, extract_terminus_options(s.source)
    assert_equal tgt_opts, extract_terminus_options(s.target)
    assert s.source.dynamic?
    assert !s.target.dynamic?
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
    server = ServerContainerThread.new("#{__method__}.server", {:idle_timeout => 0.1})
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
    server.join                 # Exits when the connection closes from idle-timeout
    client_handler.block.push nil   # Unblock the client
    ex = assert_raises(Qpid::Proton::Condition) { client_thread.join }
    assert_match(/resource-limit-exceeded/, ex.to_s)
  end

  # Make sure we stop and clean up if an aborted connection causes a handler to raise.
  # https://issues.apache.org/jira/browse/PROTON-1791
  def test_handler_raise
    cont = ServerContainer.new(__method__, {}, 0) # Don't auto-close the listener
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

  # Check if two time values are "close enough" to be reasonable.
  def assert_equalish(x, y, delta=0.1)
    assert_in_delta(x, y, delta)
  end

  # Test container doesn't stops only when schedule work is done
  def test_container_work_queue
    c = Container.new __method__
    delays = [0.1, 0.03, 0.02]
    a = []
    delays.each { |d| c.schedule(d) { a << [d, Time.now] } }
    start = Time.now
    c.run
    delays.sort.each do |d|
      x = a.shift
      assert_equal d, x[0]
    end
    assert_equalish delays.reduce(:+), Time.now-start
  end

  # Test container work queue finishes due tasks on external stop, drops future tasks
  def test_container_work_queue_stop
    q = Queue.new
    c = Container.new __method__
    thread = Thread.new { c.run }
    time = Time.now + 0.01
    # Mix good scheduled tasks at time and bad tasks scheduled after 10 secs
    10.times do
      c.schedule(time) { q << true }
      c.schedule(10) { q << false }
    end
    assert_same true, q.pop # First task processed, all others at same time are due
    # Mix in some immediate tasks
    5.times do
      c.work_queue.add { q << true } # Immediate
      c.schedule(time) { q << true }
      c.schedule(10) { q << false }
    end
    c.stop
    thread.join
    19.times { assert_same true, q.pop }
    assert_equal 0, q.size      # Tasks with 10 sec delay should be dropped
  end

  # Chain schedule calls from other schedule calls
  def test_container_schedule_chain
    c = Container.new(__method__)
    delays = [0.05, 0.02, 0.04]
    i = delays.each
    a = []
    p = Proc.new { c.schedule(i.next) { a << Time.now; p.call } rescue nil }
    p.call                 # Schedule the first, which schedules the second etc.
    start = Time.now
    c.run
    assert_equal 3, a.size
    assert_equalish delays.reduce(:+), Time.now-start
  end

  # Schedule calls from handlers
  def test_container_schedule_handler
    h = Class.new() do
      def initialize() @got = []; end
      attr_reader :got
      def record(m) @got << m; end
      def on_container_start(c) c.schedule(0) {record __method__}; end
      def on_connection_open(c) c.close; c.container.schedule(0) {record __method__}; end
      def on_connection_close(c) c.container.schedule(0) {record __method__}; end
    end.new
    t = ServerContainerThread.new(__method__, nil, 1, h)
    t.connect(t.url)
    t.join
    assert_equal [:on_container_start, :on_connection_open, :on_connection_open, :on_connection_close, :on_connection_close], h.got
  end

  # Raising from container handler should stop container
  def test_container_handler_raise
    h = Class.new() do
      def on_container_start(c) raise "BROKEN"; end
    end.new
    c = Container.new(h, __method__)
    assert_equal("BROKEN", (assert_raises(RuntimeError) { c.run }).to_s)
  end

  # Raising from connection handler should stop container
  def test_connection_handler_raise
    h = Class.new() do
      def on_connection_open(c) raise "BROKEN"; end
    end.new
    c = ServerContainer.new(__method__, nil, 1, h)
    c.connect(c.url)
    assert_equal("BROKEN", (assert_raises(RuntimeError) { c.run }).to_s)
  end

  # Raising from container schedule should stop container
  def test_container_schedule_raise
    c = Container.new(__method__)
    c.schedule(0) { raise "BROKEN" }
    assert_equal("BROKEN", (assert_raises(RuntimeError) { c.run }).to_s)
  end

  def test_connection_work_queue
    cont = ServerContainer.new(__method__, {}, 1)
    c = cont.connect(cont.url)
    t = Thread.new { cont.run }
    q = Queue.new

    start = Time.now
    c.work_queue.schedule(0.02) { q << [3, Thread.current] }
    c.work_queue.add { q << [1, Thread.current] }
    c.work_queue.schedule(0.04) { q << [4, Thread.current] }
    c.work_queue.add { q << [2, Thread.current] }

    assert_equal [1, t], q.pop
    assert_equal [2, t], q.pop
    assert_equalish 0.0, Time.now-start
    assert_equal [3, t], q.pop
    assert_equal [4, t], q.pop
    assert_equalish 0.02 + 0.04, Time.now-start

    c.work_queue.add { c.close }
    t.join
    assert_raises(WorkQueue::StoppedError) { c.work_queue.add {  } }
  end

  # Raising from connection schedule should stop container
  def test_connection_work_queue_raise
    c = ServerContainer.new(__method__)
    c.connect(c.url)
    c.work_queue.add { raise "BROKEN" }
    assert_equal("BROKEN", (assert_raises(RuntimeError) { c.run }).to_s)
  end
end
