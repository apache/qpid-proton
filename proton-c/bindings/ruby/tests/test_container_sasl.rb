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

  def initialize(handler, listener_opts, id)
    super handler, id
    @listener = listen_io(TCPServer.open(0), ListenOnceHandler.new(listener_opts))
  end
  attr_reader :listener
end

class ContainerSASLTest < MiniTest::Test
  include Qpid::Proton

  # Handler for test client/server that sets up server and client SASL options
  class SASLHandler < TestHandler

    def initialize(url="amqp://", opts=nil)
      super()
      @url, @opts = url, opts
    end

    def on_container_start(container)
      @client = container.connect("#{@url}:#{container.listener.port}", @opts)
    end

    attr_reader :auth_user

    def on_connection_open(connection)
      super
      if connection == @client
        connection.close
      else
        @auth_user = connection.user
      end
    end
  end

  # Generate SASL server configuration files and database, initialize proton SASL
  class SASLConfig
    include Qpid::Proton
    attr_reader :conf_dir, :conf_file, :conf_name, :database, :error

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
        SASL.config_path =  conf_dir
        SASL.config_name = conf_name
      end
    rescue => e
      @error = e
    end

    private

    SASLPASSWD = (ENV['SASLPASSWD'] or 'saslpasswd2')

    def make_user(user, password, realm=nil)
      realm_opt = (realm ? "-u #{realm}" : "")
      cmd = "echo '#{password}' | #{SASLPASSWD} -c -p -f #{database} #{realm_opt} #{user}"
      system(cmd) or raise RuntimeError.new("saslpasswd2 failed: #{cmd}")
    end

    INSTANCE = SASLConfig.new
  end

  def begin_extended_test
    skip("Extended SASL not enabled") unless SASL.extended?
    skip("Extended SASL setup error: #{SASLConfig::INSTANCE.error}") if SASLConfig::INSTANCE.error
  end

  def test_sasl_anonymous()
    s = SASLHandler.new("amqp://",  {:sasl_allowed_mechs => "ANONYMOUS"})
    TestContainer.new(s, {:sasl_allowed_mechs => "ANONYMOUS"}, __method__).run
    assert_equal "anonymous", s.connections[0].user
  end

  def test_sasl_plain_url()
    begin_extended_test
    # Use default realm with URL, should authenticate with "default_password"
    opts = {:sasl_allowed_mechs => "PLAIN", :sasl_allow_insecure_mechs => true}
    s = SASLHandler.new("amqp://user:default_password@",  opts)
    TestContainer.new(s, opts, __method__).run
    assert_equal(2, s.connections.size)
    assert_equal("user", s.auth_user)
  end

  def test_sasl_plain_options()
    begin_extended_test
    # Use default realm with connection options, should authenticate with "default_password"
    opts = {:sasl_allowed_mechs => "PLAIN",:sasl_allow_insecure_mechs => true,
            :user => 'user', :password => 'default_password' }
    s = SASLHandler.new("amqp://", opts)
    TestContainer.new(s, {:sasl_allowed_mechs => "PLAIN",:sasl_allow_insecure_mechs => true}, __method__).run
    assert_equal(2, s.connections.size)
    assert_equal("user", s.auth_user)
  end

  # Ensure we don't allow PLAIN if allow_insecure_mechs = true is not explicitly set
  def test_disallow_insecure()
    # Don't set allow_insecure_mechs, but try to use PLAIN
    s = SASLHandler.new("amqp://user:password@", {:sasl_allowed_mechs => "PLAIN", :sasl_allow_insecure_mechs => true})
    e = assert_raises(TestError) { TestContainer.new(s, {:sasl_allowed_mechs => "PLAIN"}, __method__).run }
    assert_match(/amqp:unauthorized-access.*Authentication failed/, e.to_s)
  end
end
