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


# Tools for tests. Only minitest is used.

require 'minitest/autorun'
require 'qpid_proton'
require 'socket'

begin
  MiniTest::Test
rescue NameError                # For older versions of MiniTest
  MiniTest::Test = MiniTest::Unit::TestCase
end

class TestError < RuntimeError; end  # Normal error
class TestException < Exception; end # Not caught by default rescue

def wait_port(port, timeout=5)
  deadline = Time.now + timeout
  begin  # Wait for the port to be connectible
    TCPSocket.open("", $port).close
  rescue Errno::ECONNREFUSED
    if Time.now > deadline then
      raise TestError, "timed out waiting for port #{port}"
    end
    sleep(0.1)
    retry
  end
end

# Handler that records some common events that are checked by tests
class TestHandler < Qpid::Proton::MessagingHandler
  attr_reader :errors, :connections, :sessions, :links, :messages

  # Pass optional extra handlers and options to the Container
  # @param raise_errors if true raise an exception for error events, if false, store them in #errors
  def initialize(raise_errors=true)
    super()
    @raise_errors = raise_errors
    @errors, @connections, @sessions, @links, @messages = 5.times.collect { [] }
  end

  # If the handler has errors, raise a TestError with all the error text
  def raise_errors()
    return if @errors.empty?
    text = ""
    while @errors.size > 0
      text << @errors.pop + "\n"
    end
    raise TestError.new("TestHandler has errors:\n #{text}")
  end

  def on_error(condition)
    @errors.push "#{condition}"
    raise_errors if @raise_errors
  end

  def endpoint_open(queue, endpoint)
    queue.push(endpoint)
  end

  def on_connection_open(c)
    endpoint_open(@connections, c)
  end

  def on_session_open(s)
    endpoint_open(@sessions, s)
  end

  def on_receiver_open(l)
    endpoint_open(@links, l)
  end

  def on_sender_open(l)
    endpoint_open(@links, l)
  end

  def on_message(d, m)
    @messages.push(m)
  end
end

# ListenHandler that closes the Listener after first (or n) accepts
class ListenOnceHandler < Qpid::Proton::Listener::Handler
  def initialize(opts, n=1) super(opts); @n=n; end
  def on_error(l, e) raise e; end
  def on_accept(l) l.close if (@n -= 1).zero?; super; end
end

# Add port/url to Listener, assuming a TCP socket
class Qpid::Proton::Listener
  def url() "amqp://:#{port}"; end
end

# A client/server pair of ConnectionDrivers linked by a socket pair
DriverPair = Struct.new(:client, :server) do

  def initialize(client_handler, server_handler)
    s = Socket.pair(:LOCAL, :STREAM, 0)
    self.client = HandlerDriver.new(s[0], client_handler)
    self.server = HandlerDriver.new(s[1], server_handler)
    server.transport.set_server
  end

  # Process each driver once, return time of next timed event
  def process(now = Time.now, max_time=nil)
    t = collect { |d| d.process(now) }.compact.min
    t =  max_time if max_time && t > max_time
    t
  end

  def active()
    can_read = self.select { |d| d.can_read? }
    can_write = self.select  {|d| d.can_write? }
    IO.select(can_read, can_write, [], 0)
  end

  def names() collect { |x| x.handler.names }; end

  def clear() each { |x| x.handler.clear; } end

  # Run till there is nothing else to do - not handle waiting for timed events
  # but does pass +now+ to process and returns the min returned timed event time
  def run(now=Time.now)
    t = nil
    begin
      t = process(now, t)
    end while active
    t
  end
end

# Container that listens on a random port
class ServerContainer < Qpid::Proton::Container
  include Qpid::Proton

  def initialize(id=nil, listener_opts=nil, n=1, handler=nil)
    super handler, id
    @listener = listen_io(TCPServer.open(0), ListenOnceHandler.new(listener_opts, n))
  end

  attr_reader :listener

  def port() @listener.port; end
  def url() "amqp://:#{port}"; end
end

class ServerContainerThread < ServerContainer
  def initialize(*args)
    super
    @thread = Thread.new { run }
  end

  attr_reader :thread
  def join() @thread.join; end
end
