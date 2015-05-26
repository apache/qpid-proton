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

class HelloWorld < Qpid::Proton::Handler::MessagingHandler

  def initialize(server, address)
    super()
    @server = server
    @address = address
  end

  def on_start(event)
    conn = event.container.connect(:address => @server)
    event.container.create_sender(conn, :target => @address)
    event.container.create_receiver(conn, :source => @address)
  end

  def on_sendable(event)
    msg = Qpid::Proton::Message.new
    msg.body = "Hello world!"
    event.sender.send(msg)
    event.sender.close
  end

  def on_message(event)
    puts event.message.body
    event.connection.close
  end
end

options = {
  :address => "localhost:5672",
  :queue => "examples"
}

OptionParser.new do |opts|
  opts.banner = "Usage: helloworld_direct.rb [options]"

  opts.on("-a", "--address=ADDRESS", "Send messages to ADDRESS (def. #{options[:address]}).") do |address|
    options[:address] = address
  end

  opts.on("-q", "--queue=QUEUE", "Send messages to QUEUE (def. #{options[:queue]})") do |queue|
    options[:queue] = queue
  end

end.parse!

hw = HelloWorld.new(options[:address], "examples")
Qpid::Proton::Reactor::Container.new(hw).run
