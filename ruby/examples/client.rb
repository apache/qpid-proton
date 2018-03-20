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

class Client < Qpid::Proton::MessagingHandler

  def initialize(url, address, requests)
    super()
    @url = url
    @address = address
    @requests = requests
  end

  def on_container_start(container)
    c = container.connect(@url)
    @sender = c.open_sender(@address)
    @receiver = c.open_receiver({:dynamic => true})
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

  def on_receiver_open(receiver)
    next_request
  end

  def on_message(delivery, message)
    puts "<- #{message.body}"
    @requests.delete_at(0)
    if !@requests.empty?
      next_request
    else
      delivery.connection.close
    end
  end

  def on_transport_error(transport)
    raise "Connection error: #{transport.condition}"
  end

end

REQUESTS = ["Twas brillig, and the slithy toves",
            "Did gire and gymble in the wabe.",
            "All mimsy were the borogroves,",
            "And the mome raths outgrabe."]

if ARGV.size != 2
  STDERR.puts "Usage: #{__FILE__} URL ADDRESS
Connect to URL and send messages to ADDRESS"
  return 1
end
url, address = ARGV
Qpid::Proton::Container.new(Client.new(url, address, REQUESTS)).run
