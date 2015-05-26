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

class Client < Qpid::Proton::Handler::MessagingHandler

  def initialize(url, requests)
    super()
    @url = url
    @requests = requests
  end

  def on_start(event)
    @sender = event.container.create_sender(@url)
    @receiver = event.container.create_receiver(@sender.connection, :dynamic => true)
  end

  def next_request
    if @receiver.remote_source.address
      req = Qpid::Proton::Message.new
      req.reply_to = @receiver.remote_source.address
      req.body = @requests.first
      puts "-> #{req.body}"
      @sender.send(req)
    end
  end

  def on_link_opened(event)
    if event.receiver == @receiver
      next_request
    end
  end

  def on_message(event)
    puts "<- #{event.message.body}"
    @requests.delete_at(0)
    if !@requests.empty?
      next_request
    else
      event.connection.close
    end
  end

end

REQUESTS = ["Twas brillig, and the slithy toves",
            "Did gire and gymble in the wabe.",
            "All mimsy were the borogroves,",
            "And the mome raths outgrabe."]

Qpid::Proton::Reactor::Container.new(Client.new("0.0.0.0:5672/examples", REQUESTS)).run
