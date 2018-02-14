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

OldMessagingHandler = Qpid::Proton::Handler::MessagingHandler #Use the old handler.

# Records every call
class AllHandler < OldMessagingHandler
  def initialize(*args)
    super(*args)
    @calls = []
  end

  attr_accessor :calls

  def names; @calls.map { |c| c[0] }; end
  def events; @calls.map { |c| c[1] }; end

  def method_missing(name, *args) (/^on_/ =~ name) ? (@calls << [name] + args) : super; end
  def respond_to_missing?(name, private=false); (/^on_/ =~ name); end
  def respond_to?(name, all=false) super || respond_to_missing?(name); end # For ruby < 1.9.2
end

# Tests with Mock handler that handles all methods, expect both old and new calls
class TestOldHandler < MiniTest::Test
  def setup
    @h = [AllHandler.new, AllHandler.new]
    @ch, @sh = *@h
    @d = DriverPair.new(*@h)
  end

  def clear; @d.each { |d| h = d.handler; h.calls.clear }; end

  def test_handler_defaults
    want = { :prefetch => 10, :auto_settle => true, :auto_accept => true, :auto_open => true, :auto_close => true, :peer_close_is_error => false }
    assert_equal want, @ch.options
    assert_equal want, @sh.options
  end

  def test_auto_open_close
    @d.client.connection.open; @d.client.connection.open_sender; @d.run
    assert_equal [:on_connection_opened, :on_session_opened, :on_link_opened, :on_sendable], @ch.names
    assert_equal [:on_connection_opening, :on_session_opening, :on_link_opening, :on_connection_opened, :on_session_opened, :on_link_opened], @sh.names
    clear
    @d.client.connection.close; @d.run
    assert_equal [:on_connection_closed, :on_transport_closed], @ch.names
    assert_equal [:on_connection_closing, :on_connection_closed, :on_transport_closed], @sh.names
  end

  def test_no_auto_open_close
    [:auto_close, :auto_open].each { |k| @ch.options[k] = @sh.options[k] = false }
    @d.client.connection.open; @d.run
    assert_equal [:on_connection_opening], @sh.names
    assert_equal [], @ch.names
    @d.server.connection.open; @d.run
    assert_equal [:on_connection_opened], @ch.names
    assert_equal [:on_connection_opening, :on_connection_opened], @sh.names
    clear
    @d.client.connection.session.open; @d.run
    assert_equal [:on_session_opening], @sh.names
    assert_equal [], @ch.names
    clear
    @d.client.connection.close;
    3.times { @d.process }
    assert_equal [:on_connection_closing], @sh.names
    assert_equal [], @ch.names
    @d.server.connection.close; @d.run
    assert_equal [:on_connection_closed, :on_transport_closed], @ch.names
    assert_equal [:on_connection_closing, :on_connection_closed, :on_transport_closed], @sh.names
  end

  def test_transport_error
    @d.client.connection.open; @d.run
    clear
    @d.client.close "stop that"; @d.run
    assert_equal [:on_transport_closed], @ch.names
    assert_equal [:on_transport_error, :on_transport_closed], @sh.names
    assert_equal Condition.new("proton:io", "stop that (connection aborted)"), @d.client.transport.condition
    assert_equal Condition.new("amqp:connection:framing-error", "connection aborted"), @d.server.transport.condition
  end

  def test_connection_error
    @ch.options[:auto_open] = @sh.options[:auto_open] = false
    @d.client.connection.open; @d.run
    @d.server.connection.close "bad dog"; @d.run
    assert_equal [:on_connection_opened, :on_connection_error, :on_connection_closed, :on_transport_closed], @ch.names
    assert_equal "bad dog", @ch.calls[2][1].condition.description
    assert_equal [:on_connection_opening, :on_connection_closed, :on_transport_closed], @sh.names
  end

  def test_session_error
    @d.client.connection.open
    s = @d.client.connection.session; s.open; @d.run
    s.close "bad dog"; @d.run
    assert_equal [:on_connection_opened, :on_session_opened, :on_session_closed], @ch.names
    assert_equal [:on_connection_opening, :on_session_opening, :on_connection_opened, :on_session_opened, :on_session_error, :on_session_closed], @sh.names
    assert_equal "bad dog", @sh.calls[-3][1].condition.description
  end

  def test_link_error
    @d.client.connection.open
    s = @d.client.connection.open_sender; @d.run
    s.close "bad dog"; @d.run
    assert_equal [:on_connection_opened, :on_session_opened, :on_link_opened, :on_sendable, :on_link_closed], @ch.names
    assert_equal [:on_connection_opening, :on_session_opening, :on_link_opening,
                  :on_connection_opened, :on_session_opened, :on_link_opened,
                  :on_link_error, :on_link_closed], @sh.names
    assert_equal "bad dog", @sh.calls[-3][1].condition.description
  end

  def test_options_off
    off = {:prefetch => 0, :auto_settle => false, :auto_accept => false, :auto_open => false, :auto_close => false}
    @ch.options.replace(off)
    @sh.options.replace(off)
    @d.client.connection.open; @d.run
    assert_equal [[], [:on_connection_opening]], [@ch.names, @sh.names]
    @d.server.connection.open; @d.run
    assert_equal [[:on_connection_opened], [:on_connection_opening, :on_connection_opened]], [@ch.names, @sh.names]
    clear
    s = @d.client.connection.open_sender; @d.run
    assert_equal [[], [:on_session_opening, :on_link_opening]], [@ch.names, @sh.names]
    @sh.events[1].session.open
    r = @sh.events[1].link
    r.open; @d.run
    assert_equal [[:on_session_opened, :on_link_opened], [:on_session_opening, :on_link_opening, :on_session_opened, :on_link_opened]], [@ch.names, @sh.names]
    clear
    r.flow(1); @d.run
    assert_equal [[:on_sendable], []], [@ch.names, @sh.names]
    assert_equal 1, s.credit
    clear
    s.send Message.new("foo"); @d.run
    assert_equal [[], [:on_message]], [@ch.names, @sh.names]
  end

  def test_peer_close_is_error
    @ch.options[:peer_close_is_error] = true
    @d.client.connection.open; @d.run
    @d.server.connection.close; @d.run
    assert_equal [:on_connection_opened, :on_connection_error, :on_connection_closed, :on_transport_closed], @ch.names
    assert_equal [:on_connection_opening, :on_connection_opened, :on_connection_closed, :on_transport_closed], @sh.names
  end
end

# Test with real handlers that implement a few methods
class TestOldUnhandled < MiniTest::Test

  def test_message
    handler_class = Class.new(OldMessagingHandler) do
      def on_message(event) @message = event.message; end
      def on_accepted(event) @accepted = true; end
      attr_accessor :message, :accepted, :sender
    end
    d = DriverPair.new(handler_class.new, handler_class.new)
    d.client.connection.open;
    s = d.client.connection.open_sender; d.run
    assert_equal 10, s.credit   # Default prefetch
    s.send(Message.new("foo")); d.run
    assert_equal "foo", d.server.handler.message.body
    assert d.client.handler.accepted
  end

  # Verify on_unhandled is called
  def test_unhandled
    handler_class = Class.new(OldMessagingHandler) do
      def initialize() super; @unhandled = []; end
      def on_unhandled(event) @unhandled << event.method; end
      attr_accessor :unhandled
    end
    d = DriverPair.new(handler_class.new, handler_class.new)
    d.client.connection.open; d.run
    assert_equal [:on_connection_opened], d.client.handler.unhandled
    assert_equal [:on_connection_opening, :on_connection_opened], d.server.handler.unhandled
  end

  # Verify on_error is called
  def test_on_error
    handler_class = Class.new(OldMessagingHandler) do
      def initialize() super; @error = []; @unhandled = []; end
      def on_error(event) @error << event.method; end
      def on_unhandled(event) @unhandled << event.method; end
      attr_accessor :error, :unhandled
    end
    d = DriverPair.new(handler_class.new, handler_class.new)
    d.client.connection.open
    r = d.client.connection.open_receiver; d.run
    r.close "oops"; d.run
    assert_equal [:on_connection_opened, :on_session_opened, :on_link_opened,
                  :on_link_closed], d.client.handler.unhandled
    assert_equal [:on_connection_opening, :on_session_opening, :on_link_opening,
                  :on_connection_opened, :on_session_opened, :on_link_opened, :on_sendable,
                  :on_link_closed], d.server.handler.unhandled
    assert_equal [:on_link_error], d.server.handler.error

  end

  # Verify on_unhandled is called even for errors if there is no on_error
  def test_unhandled_error
    handler_class = Class.new(OldMessagingHandler) do
      def initialize() super; @unhandled = []; end
      def on_unhandled(event) @unhandled << event.method; end
      attr_accessor :unhandled
    end
    d = DriverPair.new(handler_class.new, handler_class.new)
    d.client.connection.open; d.run
    d.client.connection.close "oops"; d.run
    assert_equal [:on_connection_opened, :on_connection_closed, :on_transport_closed], d.client.handler.unhandled
    assert_equal [:on_connection_opening, :on_connection_opened, :on_connection_error, :on_connection_closed, :on_transport_closed], d.server.handler.unhandled
  end
end
