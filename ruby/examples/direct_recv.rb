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

class DirectReceive < Qpid::Proton::MessagingHandler

  def initialize(url, address, count)
    super()
    @url = url
    @address = address
    @expected = count
    @received = 0
  end

  class ListenOnce < Qpid::Proton::Listener::Handler
    def on_open(l) STDOUT.puts "Listening on #{l.port}\n"; STDOUT.flush; end
    def on_accept(l) l.close; end
  end

  def on_container_start(container)
    container.listen(@url, ListenOnce.new)
  end

  def on_message(delivery, message)
    if @expected.zero? || (@received < @expected)
      puts "Received: #{message.body}"
      @received = @received + 1
      if @received == @expected
        delivery.connection.close
      end
    end
  end
end

unless (2..3).include? ARGV.size
  STDERR.puts "Usage: #{__FILE__} URL ADDRESS [COUNT]
Listen on URL and receive COUNT messages from ADDRESS"
  return 1
end
url, address, count = ARGV
count = Integer(count || 10)
Qpid::Proton::Container.new(DirectReceive.new(url, address, count)).run

