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

Message = Qpid::Proton::Message
SASL = Qpid::Proton::SASL
URL = Qpid::Proton::URL

class ContainerTest < Minitest::Test

  # Send n messages
  class SendMessageClient < TestHandler
    attr_reader :accepted

    def initialize(url, link_name, body)
      super()
      @url, @link_name, @message = url, link_name, Message.new(body)
    end

    def on_start(event)
      event.container.create_sender(@url, {:name => @link_name})
    end

    def on_sendable(event)
      if event.sender.credit > 0
        event.sender.send(@message)
      end
    end

    def on_accepted(event)
      @accepted = event
      event.connection.close
    end
  end

  def test_simple()
    TestServer.new.run do |s|
      lname = "test-link"
      body = "hello"
      c = SendMessageClient.new(s.addr, lname, body).run
      assert_instance_of(Qpid::Proton::Event::Event, c.accepted)
      assert_equal(lname, s.links.pop(true).name)
      assert_equal(body, s.messages.pop(true).body)
    end
  end

end

class ContainerSASLTest < Minitest::Test

  # Connect to URL using mechanisms and insecure to configure the transport
  class SASLClient < TestHandler

    def initialize(url, opts={})
      super()
      @url, @opts = url, opts
    end

    def on_start(event)
      event.container.connect(@url, @opts)
    end

    def on_connection_opened(event)
      super
      event.container.stop
    end
  end

  # Server with SASL settings
  class SASLServer < TestServer
    def initialize(mechanisms=nil, insecure=nil, realm=nil)
      super()
      @mechanisms, @insecure, @realm = mechanisms, insecure, realm
    end

    def on_connection_bound(event)
      sasl = event.transport.sasl
      sasl.allow_insecure_mechs = @insecure unless @insecure.nil?
      sasl.allowed_mechs = @mechanisms unless @mechanisms.nil?
      # TODO aconway 2017-08-16: need `sasl.realm(@realm)` here for non-default realms.
      # That reqiures pn_sasl_set_realm() at the C layer - the realm should
      # be passed to cyrus_sasl_init_server()
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
    SASLServer.new("ANONYMOUS").run do |s|
      c = SASLClient.new(s.addr, {:sasl_allowed_mechs => "ANONYMOUS"}).run
      refute_empty(c.connections)
      refute_empty(s.connections)
      assert_nil(s.connections.pop(true).user)
    end
  end

  def test_sasl_plain_url()
    # Use default realm with URL, should authenticate with "default_password"
    SASLServer.new("PLAIN", true).run do |s|
      c = SASLClient.new("amqp://user:default_password@#{s.addr}",
                         {:sasl_allowed_mechs => "PLAIN", :sasl_allow_insecure_mechs => true}).run
      refute_empty(c.connections)
      refute_empty(s.connections)
      sc = s.connections.pop(true)
      assert_equal("user", sc.transport.sasl.user)
    end
  end

  def test_sasl_plain_options()
    # Use default realm with connection options, should authenticate with "default_password"
    SASLServer.new("PLAIN", true).run do |s|
      c = SASLClient.new(s.addr,
                         {:user => "user", :password => "default_password",
                          :sasl_allowed_mechs => "PLAIN", :sasl_allow_insecure_mechs => true}).run
      refute_empty(c.connections)
      refute_empty(s.connections)
      sc = s.connections.pop(true)
      assert_equal("user", sc.transport.sasl.user)
    end
  end

  # Test disabled, see on_connection_bound - missing realm support in proton C.
  def TODO_test_sasl_plain_realm()
    # Use the non-default proton realm on the server, should authenticate with "password"
    SASLServer.new("PLAIN", true, "proton").run do |s|
      c = SASLClient.new("amqp://user:password@#{s.addr}",
                         {:sasl_allowed_mechs => "PLAIN", :sasl_allow_insecure_mechs => true}).run
      refute_empty(c.connections)
      refute_empty(s.connections)
      sc = s.connections.pop(true)
      assert_equal("user", sc.transport.sasl.user)
    end
  end

  # Ensure we don't allow PLAIN if allow_insecure_mechs = true is not explicitly set
  def test_disallow_insecure()
    # Don't set allow_insecure_mechs, but try to use PLAIN
    SASLServer.new("PLAIN", nil).run(true) do |s|
      begin
        SASLClient.new("amqp://user:password@#{s.addr}",
                       {:sasl_allowed_mechs => "PLAIN", :sasl_allow_insecure_mechs => true}).run
      rescue TestError => e
        assert_match(/PN_TRANSPORT_ERROR.*unauthorized-access/, e.to_s)
      end
    end
  end
end
