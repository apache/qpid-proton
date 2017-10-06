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
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#++

# Tools for tests. Only minitest is used.

require 'minitest/autorun'
require 'qpid_proton'
require 'thread'
require 'socket'

Container = Qpid::Proton::Reactor::Container
MessagingHandler = Qpid::Proton::Handler::MessagingHandler

# Bind an unused local port using bind(0) and SO_REUSEADDR and hold it till close()
# Provides #host, #port and #addr ("host:port") as strings
class TestPort
  attr_reader :host, :port, :addr

  # With block, execute block passing self then close
  # Note host must be the local host, but you can pass '::1' instead for ipv6
  def initialize(host='127.0.0.1')
    @sock = Socket.new(:INET, :STREAM)
    @sock.setsockopt(Socket::SOL_SOCKET, Socket::SO_REUSEADDR, true)
    @sock.bind(Socket.sockaddr_in(0, host))
    @host, @port = @sock.connect_address.ip_unpack
    @addr = "#{@host}:#{@port}"
    if block_given?
      begin
        yield self
      ensure
        close
      end
    end
  end

  def close() @sock.close(); end
end

class TestError < Exception; end

def wait_port(port, timeout=5)
  deadline = Time.now + timeout
  begin  # Wait for the port to be connectible
    TCPSocket.open("", $port).close
  rescue Errno::ECONNREFUSED
    if Time.now > deadline then
      raise TestError("timed out waiting for port #{port}")
    end
    sleep(0.1)
    retry
  end
end

# Handler that records some common events that are checked by tests
class TestHandler < MessagingHandler

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

  # TODO aconway 2017-08-15: implement in MessagingHandler
  def on_error(event, endpoint)
    @errors.push "#{event.type}: #{endpoint.condition.name}: #{endpoint.condition.description}"
    raise_errors if @raise_errors
  end

  def on_transport_error(event)
    on_error(event, event.transport)
  end

  def on_connection_error(event)
    on_error(event, event.condition)
  end

  def on_session_error(event)
    on_error(event, event.session)
  end

  def on_link_error(event)
    on_error(event, event.link)
  end

  def on_opened(queue, endpoint)
    queue.push(endpoint)
    endpoint.open
  end

  def on_connection_opened(event)
    on_opened(@connections, event.connection)
  end

  def on_session_opened(event)
    on_opened(@sessions, event.session)
  end

  def on_link_opened(event)
    on_opened(@links, event.link)
  end

  def on_message(event)
    @messages.push(event.message)
  end
end

# A TestHandler that listens on a TestPort
class TestServer < TestHandler
  def initialize
    super
    @tp = TestPort.new
  end

  def host() @tp.host;  end
  def port() @tp.port;  end
  def addr() @tp.addr;  end

  def on_start(e)
    super
    @listener = e.container.listen(addr)
  end
end
