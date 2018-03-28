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

# Records every call, never provokes "on_unhandled"
class RecordingHandler < Qpid::Proton::MessagingHandler

  def initialize(*args) super(*args); @calls = []; end

  attr_accessor :calls

  def names() @calls.collect { |c| c[0] }; end

  def clear() @calls.clear; end

  def method_missing(name, *args)
    respond_to_missing?(name) ? (@calls << [name, *args]) : super;
  end
  def respond_to_missing?(name, private=false); (/^on_/ =~ name); end
  def respond_to?(name, all=false) super || respond_to_missing?(name); end # For ruby < 1.9.2
end

class NoAutoOpenClose < RecordingHandler
  def initialize() super; @endpoints = []; end
  def on_connection_open(x) @connection = x; super; raise StopAutoResponse; end
  def on_session_open(x) @session = x; super; raise StopAutoResponse; end
  def on_sender_open(x) @link = x; super; raise StopAutoResponse; end
  def on_receiver_open(x) @link = x; super; raise StopAutoResponse; end
  def on_connection_close(x) super; raise StopAutoResponse; end
  def on_session_close(x) super; raise StopAutoResponse; end
  def on_sender_close(x) super; raise StopAutoResponse; end
  def on_receiver_close(x) super; raise StopAutoResponse; end
  attr_reader :connection, :session, :link
end

class TestMessagingHandler < MiniTest::Test

  def test_auto_open_close
    d = DriverPair.new(RecordingHandler.new, RecordingHandler.new)
    d.client.connection.open; d.client.connection.open_sender; d.run
    assert_equal [:on_connection_open, :on_session_open, :on_sender_open, :on_sendable], d.client.handler.names
    assert_equal [:on_connection_open, :on_session_open, :on_receiver_open], d.server.handler.names
    d.clear
    d.client.connection.close; d.run
    assert_equal [:on_connection_close, :on_transport_close], d.server.handler.names
    assert_equal [:on_connection_close, :on_transport_close], d.client.handler.names
  end

  def test_no_auto_open_close
    d = DriverPair.new(NoAutoOpenClose.new, NoAutoOpenClose.new)
    d.client.connection.open; d.run
    assert_equal [:on_connection_open], d.server.handler.names
    assert_equal [], d.client.handler.names
    d.server.connection.open; d.run
    assert_equal [:on_connection_open], d.client.handler.names
    assert_equal [:on_connection_open], d.server.handler.names
    d.clear
    d.client.connection.open_session; d.run
    assert_equal [:on_session_open], d.server.handler.names
    assert_equal [], d.client.handler.names
    d.clear
    d.client.connection.close;
    3.times { d.process }
    assert_equal [:on_connection_close], d.server.handler.names
    assert_equal [], d.client.handler.names
    d.server.connection.close; d.run
    assert_equal [:on_connection_close, :on_transport_close], d.client.handler.names
    assert_equal [:on_connection_close, :on_transport_close], d.server.handler.names
  end

  def test_transport_error
    d = DriverPair.new(RecordingHandler.new, RecordingHandler.new)
    d.client.connection.open; d.run
    d.clear
    d.client.close "stop that"; d.run
    assert_equal [:on_transport_close], d.client.handler.names
    assert_equal [:on_transport_error, :on_transport_close], d.server.handler.names
    assert_equal Condition.new("proton:io", "stop that (connection aborted)"), d.client.transport.condition
    assert_equal Condition.new("amqp:connection:framing-error", "connection aborted"), d.server.transport.condition
  end

  # Close on half-open
  def test_connection_error
    d = DriverPair.new(NoAutoOpenClose.new, NoAutoOpenClose.new)
    d.client.connection.open; d.run
    d.server.connection.close "bad dog"; d.run
    d.client.connection.close; d.run
    assert_equal [:on_connection_open, :on_connection_error, :on_connection_close, :on_transport_close], d.client.handler.names
    assert_equal "bad dog", d.client.handler.calls[1][1].condition.description
    assert_equal [:on_connection_open, :on_connection_error, :on_connection_close, :on_transport_close], d.server.handler.names
  end

  def test_session_error
    d = DriverPair.new(RecordingHandler.new, RecordingHandler.new)
    d.client.connection.open
    s = d.client.connection.default_session; s.open; d.run
    assert_equal [:on_connection_open, :on_session_open], d.client.handler.names
    assert_equal [:on_connection_open, :on_session_open], d.server.handler.names
    d.clear
    s.close "bad dog"; d.run
    assert_equal [:on_session_error, :on_session_close], d.client.handler.names
    assert_equal [:on_session_error, :on_session_close], d.server.handler.names
    assert_equal "bad dog", d.server.handler.calls[0][1].condition.description
  end

  def test_sender_receiver_error
    d = DriverPair.new(RecordingHandler.new, RecordingHandler.new)
    d.client.connection.open
    s = d.client.connection.open_sender; d.run
    assert_equal [:on_connection_open, :on_session_open, :on_sender_open, :on_sendable], d.client.handler.names
    assert_equal [:on_connection_open, :on_session_open, :on_receiver_open], d.server.handler.names
    d.clear
    s.close "bad dog"; d.run
    assert_equal [:on_sender_error, :on_sender_close], d.client.handler.names
    assert_equal [:on_receiver_error, :on_receiver_close], d.server.handler.names
    assert_equal "bad dog", d.server.handler.calls[0][1].condition.description
  end

  def test_options_off
    linkopts = {:credit_window=>0, :auto_settle=>false, :auto_accept=>false}
    d = DriverPair.new(NoAutoOpenClose.new, NoAutoOpenClose.new)
    d.client.connection.open; d.run
    assert_equal [[], [:on_connection_open]], d.names
    d.server.connection.open; d.run
    assert_equal [[:on_connection_open], [:on_connection_open]], d.names
    d.clear
    s = d.client.connection.open_sender(linkopts); d.run
    assert_equal [[], [:on_session_open, :on_receiver_open]], d.names
    d.server.handler.session.open      # Return session open
    d.server.handler.link.open(linkopts) # Return link open
    d.run
    assert_equal [[:on_session_open, :on_sender_open], [:on_session_open, :on_receiver_open]], d.names
    d.clear
    d.server.handler.link.flow(1); d.run
    assert_equal [[:on_sendable], []], d.names
    assert_equal 1, s.credit
    d.clear
    s.send Message.new("foo"); d.run
    assert_equal [[], [:on_message]], d.names
  end

  def test_message
    handler_class = Class.new(MessagingHandler) do
      def on_message(delivery, message) @message = message; end
      def on_tracker_accept(event) @accepted = true; end
      attr_accessor :message, :accepted
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
    handler_class = Class.new(MessagingHandler) do
      def initialize() super; @unhandled = []; end
      def on_unhandled(method, *args) @unhandled << method; end
      attr_accessor :unhandled
    end
    d = DriverPair.new(handler_class.new, handler_class.new)
    d.client.connection.open; d.run
    assert_equal [:on_connection_open], d.client.handler.unhandled
    assert_equal [:on_connection_open], d.server.handler.unhandled
  end

  # Verify on_error is called
  def test_on_error
    handler_class = Class.new(MessagingHandler) do
      def initialize() super; @error = []; @unhandled = []; end
      def on_error(condition) @error << condition; end
      def on_unhandled(method, *args) @unhandled << method; end
      attr_accessor :error, :unhandled
    end
    d = DriverPair.new(handler_class.new, handler_class.new)
    d.client.connection.open
    r = d.client.connection.open_receiver; d.run
    assert_equal [:on_connection_open, :on_session_open, :on_receiver_open], d.client.handler.unhandled
    assert_equal [:on_connection_open, :on_session_open, :on_sender_open, :on_sendable], d.server.handler.unhandled
    r.close Condition.new("goof", "oops"); d.run

    assert_equal [Condition.new("goof", "oops")], d.client.handler.error
    assert_equal [:on_connection_open, :on_session_open, :on_sender_open, :on_sendable, :on_sender_close], d.server.handler.unhandled
    assert_equal [Condition.new("goof", "oops")], d.server.handler.error

  end

  # Verify on_unhandled is called for errors if there is no on_error
  def test_unhandled_error
    handler_class = Class.new(MessagingHandler) do
      def on_unhandled(method, *args)
        @error = args[0].condition if method == :on_connection_error;
      end
      attr_accessor :error
    end
    d = DriverPair.new(handler_class.new, handler_class.new)
    d.client.connection.open; d.run
    d.client.connection.close "oops"; d.run
    assert_equal [Condition.new("error", "oops")]*2, d.collect { |x| x.handler.error }
  end
end
