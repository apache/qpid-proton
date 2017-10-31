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

require 'test_tools'
require 'minitest/unit'
require 'socket'

Message = Qpid::Proton::Message
SASL = Qpid::Proton::SASL
Disposition = Qpid::Proton::Disposition

# Container that listens on a random port
class TestContainer < Container

  def initialize(opts = {}, lopts = {})
    super opts
    @server = TCPServer.open(0)
    @listener = listen_io(@server, ListenOnceHandler.new(lopts))
  end

  def port() @server.addr[1]; end
  def url() "amqp://:#{port}"; end
end

class ContainerTest < Minitest::Test

  def test_simple()
    sh = Class.new(MessagingHandler) do
      attr_reader :accepted, :sent
      def on_sendable(e)
        e.link.send Message.new("foo") unless @sent
        @sent = true
      end

      def on_accepted(e)
        @accepted = true
        e.connection.close
      end
    end.new

    rh = Class.new(MessagingHandler) do
      attr_reader :message, :link
      def on_link_opening(e)
        @link = e.link
        e.link.open
        e.link.flow(1)
      end

      def on_message(e)
        @message = e.message;
        e.delivery.update Disposition::ACCEPTED
        e.delivery.settle
      end
    end.new

    c = TestContainer.new({:id => __method__.to_s, :handler => rh})
    c.connect(c.url, {:handler => sh}).open_sender({:name => "testlink"})
    c.run

    assert sh.accepted
    assert_equal "testlink", rh.link.name
    assert_equal "foo", rh.message.body
    assert_equal "test_simple", rh.link.connection.container_id
  end

  class CloseOnOpenHandler < TestHandler
    def on_connection_opened(e) e.connection.close; end
    def on_connection_closing(e) e.connection.close; end
  end

  def test_auto_stop
    c1 = Container.new "#{__method__}1"
    c2 = Container.new "#{__method__}2"

    # A listener and a connection
    t1 = 3.times.collect { Thread.new { c1.run } }
    l = c1.listen_io(TCPServer.new(0), ListenOnceHandler.new({ :handler => CloseOnOpenHandler.new}))
    c1.connect("amqp://:#{l.to_io.addr[1]}", { :handler => CloseOnOpenHandler.new} )
    t1.each { |t| assert t.join(1) }

    # Connect between different containers, c2 has only a connection
    t1 = Thread.new { c1.run }
    l = c1.listen_io(TCPServer.new(0), ListenOnceHandler.new({ :handler => CloseOnOpenHandler.new}))
    t2 = Thread.new {c2.run }
    c2.connect("amqp://:#{l.to_io.addr[1]}", { :handler => CloseOnOpenHandler.new} )
    assert t2.join(1)
    assert t1.join(1)
  end

  def test_auto_stop_listener_only
    c1 = Container.new "#{__method__}1"
    # Listener only, external close
    t1 = Thread.new { c1.run }
    l = c1.listen_io(TCPServer.new(0))
    l.close
    assert t1.join(1)
  end

  def test_stop
    c = Container.new __method__
    c.auto_stop = false
    l = c.listen_io(TCPServer.new(0))
    c.connect("amqp://:#{l.to_io.addr[1]}")
    threads = 5.times.collect { Thread.new { c.run } }
    assert_nil threads[0].join(0.001)
    c.stop
    threads.each { |t| assert t.join(1) }
    assert c.auto_stop          # Set by stop

    # Stop an empty container
    threads = 5.times.collect { Thread.new { c.run } }
    assert_nil threads[0].join(0.001)
    c.stop
    threads.each { |t| assert t.join(1) }
  end

end


class ContainerSASLTest < Minitest::Test

  # Handler for test client/server that sets up server and client SASL options
  class SASLHandler < TestHandler

    def initialize(url="amqp://", opts={}, mechanisms=nil, insecure=nil, realm=nil)
      super()
      @url, @opts, @mechanisms, @insecure, @realm = url, opts, mechanisms, insecure, realm
    end

    def on_start(e)
      super
      @client = e.container.connect("#{@url}:#{e.container.port}", @opts)
    end

    def on_connection_bound(e)
      if e.connection != @client # Incoming server connection
        sasl = e.transport.sasl
        sasl.allow_insecure_mechs = @insecure unless @insecure.nil?
        sasl.allowed_mechs = @mechanisms unless @mechanisms.nil?
        # TODO aconway 2017-08-16: need `sasl.realm(@realm)` here for non-default realms.
        # That reqiures pn_sasl_set_realm() at the C layer - the realm should
        # be passed to cyrus_sasl_init_server()
      end
    end

    attr_reader :auth_user

    def on_connection_opened(e)
      super
      if e.connection == @client
        e.connection.close
      else
        @auth_user = e.transport.sasl.user
      end
    end
  end

  # Generate SASL server configuration files and database, initialize proton SASL
  class SASLConfig
    attr_reader :conf_dir, :conf_file, :conf_name, :database

    def initialize()
      if SASL.extended? # Configure cyrus SASL
        @conf_dir = File.expand_path('sasl_conf')
        @conf_name = "proton-server"
        @database = File.join(@conf_dir, "proton.sasldb")
        @conf_file = File.join(conf_dir,"#{@conf_name}.conf")
        Dir::mkdir(@conf_dir) unless File.directory?(@conf_dir)
        # Same user name in different realms
        make_user("user", "password", "proton") # proton realm
        make_user("user", "default_password") # Default realm
        File.open(@conf_file, 'w') do |f|
          f.write("
sasldb_path: #{database}
mech_list: EXTERNAL DIGEST-MD5 SCRAM-SHA-1 CRAM-MD5 PLAIN ANONYMOUS
                  ")
        end
        # Tell proton library to use the new configuration
        SASL.config_path(conf_dir)
        SASL.config_name(conf_name)
      end
    end

    private

    SASLPASSWD = (ENV['SASLPASSWD'] or 'saslpasswd2')

    def make_user(user, password, realm=nil)
      realm_opt = (realm ? "-u #{realm}" : "")
      cmd = "echo '#{password}' | #{SASLPASSWD} -c -p -f #{database} #{realm_opt} #{user}"
      system(cmd) or raise RuntimeError.new("saslpasswd2 failed: #{makepw_cmd}")
    end
    DEFAULT = SASLConfig.new
  end

  def test_sasl_anonymous()
    s = SASLHandler.new("amqp://",  {:sasl_allowed_mechs => "ANONYMOUS"})
    TestContainer.new({:id => __method__.to_s, :handler => s}, {:sasl_allowed_mechs => "ANONYMOUS"}).run
    assert_nil(s.connections[0].user)
  end

  def test_sasl_plain_url()
    skip unless SASL.extended?
    # Use default realm with URL, should authenticate with "default_password"
    opts = {:sasl_allowed_mechs => "PLAIN", :sasl_allow_insecure_mechs => true}
    s = SASLHandler.new("amqp://user:default_password@",  opts)
    TestContainer.new({:id => __method__.to_s, :handler => s}, opts).run
    assert_equal(2, s.connections.size)
    assert_equal("user", s.auth_user)
  end

  def test_sasl_plain_options()
    skip unless SASL.extended?
    # Use default realm with connection options, should authenticate with "default_password"
    opts = {:sasl_allowed_mechs => "PLAIN",:sasl_allow_insecure_mechs => true,
            :user => 'user', :password => 'default_password' }
    s = SASLHandler.new("amqp://", opts)
    TestContainer.new({:id => __method__.to_s, :handler => s}, {:sasl_allowed_mechs => "PLAIN",:sasl_allow_insecure_mechs => true}).run
    assert_equal(2, s.connections.size)
    assert_equal("user", s.auth_user)
  end

  # Ensure we don't allow PLAIN if allow_insecure_mechs = true is not explicitly set
  def test_disallow_insecure()
    # Don't set allow_insecure_mechs, but try to use PLAIN
    s = SASLHandler.new("amqp://user:password@", {:sasl_allowed_mechs => "PLAIN", :sasl_allow_insecure_mechs => true})
    e = assert_raises(TestError) { TestContainer.new({:id => __method__.to_s, :handler => s}, {:sasl_allowed_mechs => "PLAIN"}).run }
    assert_match(/PN_TRANSPORT_ERROR.*unauthorized-access/, e.to_s)
  end
end

