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

options = {
  :address => "localhost:5672/examples",
  :messages => 100,
}

class SimpleSend < Qpid::Proton::Handler::MessagingHandler

  def initialize(url, expected)
    super()
    @url = url
    @sent = 0
    @confirmed = 0
    @expected = expected
  end

  def on_start(event)
    @acceptor = event.container.listen(@url)
  end

  def on_sendable(event)
    while event.sender.credit > 0 && @sent < @expected
      msg = Qpid::Proton::Message.new("sequence #{@sent}", { :id => @sent } )
      event.sender.send(msg)
      @sent = @sent + 1
    end
  end

  def on_accepted(event)
    @confirmed = @confirmed + 1
    if @confirmed == @expected
      puts "All #{@expected} messages confirmed!"
      event.connection.close
    end
  end
end

OptionParser.new do |opts|
  opts.banner = "Usage: simple_send.rb [options]"

  opts.on("-a", "--address=ADDRESS", "Send messages to ADDRESS (def. #{options[:address]}).") do |address|
    options[:address] = address
  end

  opts.on("-m", "--messages=COUNT", "The number of messages to send (def. #{options[:messages]}",
    OptionParser::DecimalInteger) do |messages|
    options[:messages] = messages
  end
end.parse!

begin
  Qpid::Proton::Reactor::Container.new(SimpleSend.new(options[:address], options[:messages])).run
rescue Interrupt => error
  puts "ERROR: #{error}"
end
