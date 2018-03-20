#!/usr/bin/env ruby
#
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
#

require 'minitest/autorun'
require 'qpid_proton'
require 'socket'
require 'rbconfig'

begin
  MiniTest::Test
rescue NameError                # For older versions of MiniTest
  MiniTest::Test = MiniTest::Unit::TestCase
end

# URL with an unused port
def test_url()
  "amqp://:#{TCPServer.open(0) { |s| s.addr[1] }}"
end


class ExampleTest < MiniTest::Test

  def run_script(*args)
    return IO.popen([ RbConfig.ruby ] + args.map { |a| a.to_s })
  end

  def assert_output(want, *args)
    assert_equal(want.strip, run_script(*args).read.strip)
  end

  def test_helloworld
    assert_output("Hello world!", "helloworld.rb", $url, "examples")
  end

  def test_client_server
    want =  <<EOS
-> Twas brillig, and the slithy toves
<- TWAS BRILLIG, AND THE SLITHY TOVES
-> Did gire and gymble in the wabe.
<- DID GIRE AND GYMBLE IN THE WABE.
-> All mimsy were the borogroves,
<- ALL MIMSY WERE THE BOROGROVES,
-> And the mome raths outgrabe.
<- AND THE MOME RATHS OUTGRABE.
EOS
    server = run_script("server.rb", $url, "examples")
    assert_output(want.strip, "client.rb", $url, "examples")
  ensure
    Process.kill :TERM, server.pid if server
  end

  def test_send_recv
    assert_output("All 10 messages confirmed!", "simple_send.rb", $url, "examples")
    want = (0..9).reduce("") { |x,y| x << "Received: sequence #{y}\n" }
    assert_output(want.strip, "simple_recv.rb", $url, "examples")
  end

  def test_ssl_send_recv
    out = run_script("ssl_send.rb", $url, "examples").read.strip
    assert_match(/Connection secured with "...*\"\nAll 10 messages confirmed!/, out)
    want = (0..9).reduce("") { |x,y| x << "Received: sequence #{y}\n" }
    assert_output(want.strip, "simple_recv.rb", $url, "examples")
  end

  def test_direct_recv
    url = test_url
      p = run_script("direct_recv.rb", url, "examples")
      p.readline                # Wait till ready
      assert_output("All 10 messages confirmed!", "simple_send.rb", url, "examples")
      want = (0..9).reduce("") { |x,y| x << "Received: sequence #{y}\n" }
      assert_equal(want.strip, p.read.strip)
  end

  def test_direct_send
    url = test_url
    p = run_script("direct_send.rb", url, "examples")
    p.readline                # Wait till ready
    want = (0..9).reduce("") { |x,y| x << "Received: sequence #{y}\n" }
    assert_output(want.strip, "simple_recv.rb", url, "examples")
    assert_equal("All 10 messages confirmed!", p.read.strip)
  end
end

# Start the broker before all tests.
$url = test_url
$broker = IO.popen([RbConfig.ruby, 'broker.rb', $url])
$broker.readline

# Kill the broker after all tests
MiniTest.after_run do
  Process.kill(:TERM, $broker.pid) if $broker
end
