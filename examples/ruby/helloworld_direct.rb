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

class HelloWorldDirect < Qpid::Proton::Handler::MessagingHandler

  include Qpid::Proton::Util::Wrapper

  def initialize(url, address)
    super()
    @url, @address = url, address
  end

  def on_start(event)
    event.container.listen(@url)
    c = event.container.connect(@url) # Connect to self!
    c.open_sender(@address)
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
    event.container.stop
  end
end

if ARGV.size != 2
  STDERR.puts "Usage: #{__FILE__} URL ADDRESS
Listen on and connect to URL (connect to self), send a message to ADDRESS and receive it back"
  return 1
end
url, address = ARGV
Qpid::Proton::Container.new(HelloWorldDirect.new(url, address)).run
