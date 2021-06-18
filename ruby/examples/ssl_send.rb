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

require 'qpid_proton'
require 'optparse'

class SimpleSend < Qpid::Proton::MessagingHandler

  def initialize(url, address, expected)
    super()
    @url = url
    @address = address
    @sent = 0
    @confirmed = 0
    @expected = expected
  end

  def on_container_start(container)
    # Use anonymous client SSL domain
    ssl_domain = Qpid::Proton::SSLDomain.new(Qpid::Proton::SSLDomain::MODE_CLIENT)
    ssl_domain.peer_authentication(Qpid::Proton::SSLDomain::ANONYMOUS_PEER)
    c = container.connect(@url, { :ssl_domain => ssl_domain })
    c.open_sender(@address)
  end

  def on_connection_open(c)
    raise "No security!"  unless c.transport.ssl?
    STDOUT.puts "Connection secured with #{c.transport.ssl.protocol_name.inspect}"
  end

  def on_sendable(sender)
    while sender.credit > 0 && @sent < @expected
      msg = Qpid::Proton::Message.new("sequence #{@sent}", { :id => @sent } )
      sender.send(msg)
      @sent = @sent + 1
    end
  end

  def on_tracker_accept(tracker)
    @confirmed = @confirmed + 1
    if @confirmed == @expected
      puts "All #{@expected} messages confirmed!"
      tracker.connection.close
    end
  end
end

unless (2..3).include? ARGV.size
  STDERR.puts "Usage: #{__FILE__} URL ADDRESS [COUNT]}
Connect to URL and send COUNT messages to ADDRESS"
  return 1
end
url, address, count = ARGV
count = Integer(count || 10)
Qpid::Proton::Container.new(SimpleSend.new(url, address, count)).run
