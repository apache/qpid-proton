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

  def test_simple()

    hc = Class.new(TestServer) do
      attr_reader :accepted

      def on_start(event)
        super
        event.container.create_sender("amqp://#{addr}", {:name => "testlink"})
      end

      def on_sendable(event)
        if @sent.nil? && event.sender.credit > 0
          event.sender.send(Message.new("testmessage"))
          @sent = true
        end
      end

      def on_accepted(event)
        @accepted = event
        event.container.stop
      end
    end
    h = hc.new
    Container.new(h).run
    assert_instance_of(Qpid::Proton::Event::Event, h.accepted)
    assert_equal "testlink", h.links.first.name
    assert_equal "testmessage", h.messages.first.body
  end
end

class ContainerSASLTest < Minitest::Test

  # Handler for test client/server that sets up server and client SASL options
  class SASLHandler < TestServer

    attr_accessor :url

    def initialize(opts={}, mechanisms=nil, insecure=nil, realm=nil)
      super()
      @opts, @mechanisms, @insecure, @realm = opts, mechanisms, insecure, realm
    end

    def on_start(e)
      super
      @client = e.container.connect(@url || "amqp://#{addr}", @opts)
    end

    def on_connection_bound(e)
      if e.connection != @client # Incoming server connection
        @listener.close
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
    s = SASLHandler.new({:sasl_allowed_mechs => "ANONYMOUS"}, "ANONYMOUS")
    Container.new(s).run
    assert_nil(s.connections[0].user)
  end

  def test_sasl_plain_url()
    # Use default realm with URL, should authenticate with "default_password"
    s = SASLHandler.new({:sasl_allowed_mechs => "PLAIN", :sasl_allow_insecure_mechs => true}, "PLAIN", true)
    s.url = ("amqp://user:default_password@#{s.addr}")
    Container.new(s).run
    assert_equal(2, s.connections.size)
    assert_equal("user", s.auth_user)
  end

  def test_sasl_plain_options()
    # Use default realm with connection options, should authenticate with "default_password"
    opts = {:sasl_allowed_mechs => "PLAIN",:sasl_allow_insecure_mechs => true,
            :user => 'user', :password => 'default_password' }
    s = SASLHandler.new(opts, "PLAIN", true)
    Container.new(s).run
    assert_equal(2, s.connections.size)
    assert_equal("user", s.auth_user)
  end

  # Ensure we don't allow PLAIN if allow_insecure_mechs = true is not explicitly set
  def test_disallow_insecure()
    # Don't set allow_insecure_mechs, but try to use PLAIN
    s = SASLHandler.new({:sasl_allowed_mechs => "PLAIN", :sasl_allow_insecure_mechs => true}, "PLAIN")
    s.url = "amqp://user:password@#{s.addr}"
    e = assert_raises(TestError) { Container.new(s).run }
    assert_match(/PN_TRANSPORT_ERROR.*unauthorized-access/, e.to_s)
  end
end
