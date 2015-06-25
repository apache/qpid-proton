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
}

class HelloWorldDirect < Qpid::Proton::Handler::MessagingHandler

  include Qpid::Proton::Util::Wrapper

  def initialize(url)
    super()
    @url = url
  end

  def on_start(event)
    @acceptor = event.container.listen(@url)
    event.container.create_sender(@url)
  end

  def on_sendable(event)
    msg = Qpid::Proton::Message.new
    msg.body = "Hello world!"
    event.sender.send(msg)
    event.sender.close
  end

  def on_message(event)
    puts "#{event.message.body}"
  end

  def on_accepted(event)
    event.connection.close
  end

  def on_connection_closed(event)
    @acceptor.close
  end

end

OptionParser.new do |opts|
  opts.banner = "Usage: helloworld_direct.rb [options]"

  opts.on("-a", "--address=ADDRESS", "Send messages to ADDRESS (def. #{options[:address]}).") do |address|
    options[:address] = address
  end

end.parse!

begin
  Qpid::Proton::Reactor::Container.new(HelloWorldDirect.new(options[:address])).run
rescue Interrupt => error
end
