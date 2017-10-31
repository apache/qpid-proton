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

class SimpleReceive < Qpid::Proton::Handler::MessagingHandler

  def initialize(url, address, count)
    super()
    @url = url
    @address = address
    @expected = count
    @received = 0
  end

  def on_start(event)
    c = event.container.connect(@url)
    c.open_receiver(@address)
  end

  def on_message(event)
    if @expected.zero? || (@received < @expected)
      puts "Received: #{event.message.body}"
      @received = @received + 1
      if @received == @expected
        event.connection.close
      end
    end
  end
end

unless (2..3).include? ARGV.size
  STDERR.puts "Usage: #{__FILE__} URL ADDRESS [COUNT]}
Connect to URL and receive COUNT messages from ADDRESS"
  return 1
end
url, address, count = ARGV

Qpid::Proton::Container.new(SimpleReceive.new(url, address, count || 10)).run

