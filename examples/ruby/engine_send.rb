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

require 'qpid_examples'
require "optparse"

DEFAULT_ADDRESS = "0.0.0.0:5672"

options = {
  :address => DEFAULT_ADDRESS,
  :debug => false,
  :verbose => false,
  :count => 1,
  :content => "This message was sent #{Time.new}"
}

OptionParser.new do |opts|
  opts.banner = "Usage: engine_recv.rb [options]"

  opts.on("-a [address]", "--address [address]",
          "The target address (def. #{DEFAULT_ADDRESS})") do |address|
    options[:address] = address
  end

  opts.on("-C [content]", "--content [content]",
          "The message content") do |content|
    options[:content] = content
  end

  opts.on("-c [count]", "--count [count]",
          "The number of messages to send (def. 1)") do |count|
    options[:count] = count.to_i
  end

  opts.on("-v", "--verbose",
          "Enable verbose output") do
    options[:verbose] = true
  end

  opts.on("-d",
          "--debug", "Enable debugging") do
    options[:debug] = true
  end

  opts.parse!
end


driver = Driver.new

conn = Qpid::Proton::Connection.new
collector = Qpid::Proton::Event::Collector.new
conn.collect(collector)

session = conn.session
conn.open
session.open

sender = session.sender("tvc_15_1")
sender.target.address = "queue"
sender.open

transport = Qpid::Proton::Transport.new
transport.bind(conn)

address, port = options[:address].split(":")

socket = TCPSocket.new(address, port)
selectable = Selectable.new(transport, socket)
sent_count = 0

sent_count = 0

driver.add(selectable)

loop do
  # let the driver process
  driver.process

  event = collector.peek

  unless event.nil?

    print "EVENT: #{event}\n" if options[:debug]

    case event.type

    when Qpid::Proton::Event::LINK_FLOW
      sender = event.sender
      credit = sender.credit

      message = Qpid::Proton::Message.new

      if credit > 0 && sent_count < options[:count]
        sent_count = sent_count.next
        message.clear
        message.address = options[:address]
        message.subject = "Message #{sent_count}..."
        message.body = options[:content]

        delivery = sender.delivery("#{sent_count}")
        sender.send(message.encode)
        delivery.settle
        sender.advance
        credit = sender.credit
      else
        sender.close
      end

    when Qpid::Proton::Event::LINK_LOCAL_CLOSE
      link = event.link
      link.close
      link.session.close

    when Qpid::Proton::Event::SESSION_LOCAL_CLOSE
      session = event.session
      session.connection.close

    when Qpid::Proton::Event::CONNECTION_LOCAL_CLOSE
      break

    end

    collector.pop
    event = collector.peek
  end
end
