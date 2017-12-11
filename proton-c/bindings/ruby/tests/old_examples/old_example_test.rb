#!/usr/bin/enc ruby
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

def unused_port; TCPServer.open(0) { |s| s.addr[1] } end
def make_url(port, path) "amqp://:#{port}/#{path}"; end

class OldExampleTest < MiniTest::Test

  def run_script(*args)
    IO.popen [RbConfig.ruby, *args];
  end

  def assert_output(want, args)
    assert_equal want.strip, run_script(*args).read.strip
  end

  def test_helloworld
    assert_output "Hello world!", ["helloworld.rb", "-a", make_url($port, __method__)]
  end

  def test_send_recv
    assert_output "All 10 messages confirmed!", ["simple_send.rb", "-a", make_url($port, __method__)]
    want = (0..9).reduce("") { |x,y| x << "Received: sequence #{y}\n" }
    assert_output want, ["simple_recv.rb", "-a", make_url($port, __method__)]
  end

  def test_smoke
    url = "127.0.0.1:#{unused_port}"
    recv = run_script("recv.rb", "~#{url}")
    recv.readline               # Wait for "Listening"
    assert_output("Status: ACCEPTED", ["send.rb", url])
    assert_equal "Got: Hello World!", recv.read.strip
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
    srv = run_script("server.rb", "-a", make_url($port, __method__))
    assert_output(want, ["client.rb", "-a", make_url($port, __method__)])
  ensure
    Process.kill :TERM, srv.pid if srv
  end

  def test_direct_recv
    url = make_url unused_port, __method__
    p = run_script("direct_recv.rb", "-a", url)
    p.readline                # Wait till ready
    assert_output("All 10 messages confirmed!", ["simple_send.rb", "-a", url])
    want = (0..9).reduce("") { |x,y| x << "Received: sequence #{y}\n" }
    assert_equal(want.strip, p.read.strip)
  end

  def test_direct_send
    url = make_url unused_port, __method__
    p = run_script("direct_send.rb", "-a", url)
    p.readline                # Wait till ready
    want = (0..9).reduce("") { |x,y| x << "Received: sequence #{y}\n" }
    assert_output(want, ["simple_recv.rb", "-a", url])
    assert_equal("All 10 messages confirmed!", p.read.strip)
  end
end

# Start the broker before all tests.
$port = unused_port
$broker = IO.popen [RbConfig.ruby, "broker.rb", "-a", ":#{$port}"]
$broker.readline                # Wait for "Listening"

# Kill the broker after all tests
MiniTest.after_run do
  Process.kill(:TERM, $broker.pid) if $broker
end
