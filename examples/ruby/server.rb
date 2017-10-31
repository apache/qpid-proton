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

class Server < Qpid::Proton::Handler::MessagingHandler

  def initialize(url, address)
    super()
    @url = url
    @address = address
    @senders = {}
  end

  def on_start(event)
    c = event.container.connect(@url)
    c.open_receiver(@address)
    @relay = nil
  end

  def on_connection_opened(event)
    if event.connection.remote_offered_capabilities &&
        event.connection.remote_offered_capabilities.contain?("ANONYMOUS-RELAY")
      @relay = event.connection.open_sender({:target => nil})
    end
  end

  def on_message(event)
    msg = event.message
    return unless msg.reply_to  # Not a request message
    puts "<- #{msg.body}"
    sender = @relay || (@senders[msg.reply_to] ||= event.connection.open_sender(msg.reply_to))
    reply = Qpid::Proton::Message.new
    reply.address = msg.reply_to
    reply.body = msg.body.upcase
    puts "-> #{reply.body}"
    reply.correlation_id = msg.correlation_id
    sender.send(reply)
  end

  def on_transport_error(event)
    raise "Connection error: #{event.transport.condition}"
  end
end

if ARGV.size != 2
  STDERR.puts "Usage: #{__FILE__} URL ADDRESS
Server listening on URL, reply to messages to ADDRESS"
  return 1
end
url, address = ARGV
Qpid::Proton::Container.new(Server.new(url, address)).run
