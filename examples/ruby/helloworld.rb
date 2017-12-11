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

class HelloWorld < Qpid::Proton::MessagingHandler

  def initialize(url, address)
    super()
    @url, @address = url, address
  end

  def on_container_start(container)
    conn = container.connect(@url)
    conn.open_sender(@address)
    conn.open_receiver(@address)
  end

  def on_sendable(sender)
    sender.send(Qpid::Proton::Message.new("Hello world!"))
    sender.close
  end

  def on_message(delivery, message)
    puts message.body
    delivery.connection.close
  end

  def on_transport_error(transport)
    raise "Connection error: #{transport.condition}"
  end
end

if ARGV.size != 2
  STDERR.puts "Usage: #{__FILE__} URL ADDRESS
Connect to URL, send a message to ADDRESS and receive it back"
  return 1
end
url, address = ARGV
Qpid::Proton::Container.new(HelloWorld.new(url, address)).run
