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

class Server < Qpid::Proton::Handler::MessagingHandler

  def initialize(url, address)
    super()
    @url = url
    @address = address
    @senders = {}
  end

  def on_start(event)
    puts "Listening on #{@url}"
    @container = event.container
    @conn = @container.connect(:address => @url)
    @receiver = @container.create_receiver(@conn, :source => @address)
    @relay = nil
  end

  def on_connection_opened(event)
    if event.connection.remote_offered_capabilities &&
      event.connection.remote_offered_capabilities.contain?("ANONYMOUS-RELAY")
      @relay = @container.create_sender(@conn, nil)
    end
  end

  def on_message(event)
    msg = event.message
    puts "<- #{msg.body}"
    sender = @relay || @senders[msg.reply_to]
    if sender.nil?
      sender = @container.create_sender(@conn, :target => msg.reply_to)
      @senders[msg.reply_to] = sender
    end
    reply = Qpid::Proton::Message.new
    reply.address = msg.reply_to
    reply.body = msg.body.upcase
    puts "-> #{reply.body}"
    reply.correlation_id = msg.correlation_id
    sender.send(reply)
  end

end

Qpid::Proton::Reactor::Container.new(Server.new("0.0.0.0:5672", "examples")).run()
