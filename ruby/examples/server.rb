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

class Server < Qpid::Proton::MessagingHandler

  def initialize(url, address)
    super()
    @url = url
    @address = address
    @senders = {}
  end

  def on_container_start(container)
    c = container.connect(@url)
    c.open_receiver(@address)
    @relay = nil
  end

  def on_connection_open(connection)
    if connection.offered_capabilities &&
        connection.offered_capabilities.include?(:"ANONYMOUS-RELAY")
      @relay = connection.open_sender({:target => nil})
    end
  end

  def on_message(delivery, message)
    return unless message.reply_to  # Not a request message
    puts "<- #{message.body}"
    unless (sender = @relay)
      sender = (@senders[message.reply_to] ||= delivery.connection.open_sender(message.reply_to))
    end
    reply = Qpid::Proton::Message.new
    reply.address = message.reply_to
    reply.body = message.body.upcase
    puts "-> #{reply.body}"
    reply.correlation_id = message.correlation_id
    sender.send(reply)
  end

  def on_transport_error(transport)
    raise "Connection error: #{transport.condition}"
  end
end

if ARGV.size != 2
  STDERR.puts "Usage: #{__FILE__} URL ADDRESS
Server listening on URL, reply to messages to ADDRESS"
  return 1
end
url, address = ARGV
Qpid::Proton::Container.new(Server.new(url, address)).run
