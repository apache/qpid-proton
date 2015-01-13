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

require "qpid_examples"
require "optparse"

DEFAULT_PORT = 5672

options = {
  :port => DEFAULT_PORT,
  :debug => false,
  :verbose => false,
}

OptionParser.new do |opts|
  opts.banner = "Usage: engine_recv.rb [options]"

  opts.on("-p [port]", "--port [port]",
          "The port to use  (def. #{DEFAULT_PORT})") do |port|
    options[:port] = port
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

server = TCPServer.new('localhost', options[:port])

last_time = Time.now

message_count = 0
driver = Driver.new

collector = Qpid::Proton::Event::Collector.new

loop do
  begin
    client = server.accept_nonblock
  rescue Errno::EAGAIN, Errno::ECONNABORTED, Errno::EINTR, Errno::EWOULDBLOCK => error

  end

  unless client.nil?
    puts "Connection from #{client.peeraddr.last}"
    connection = Qpid::Proton::Connection.new
    connection.collect(collector)
    transport = Qpid::Proton::Transport.new(Qpid::Proton::Transport::SERVER)
    transport.bind(connection)
    selectable = Selectable.new(transport, client)
    driver.add(selectable)
  end

  # let the driver process data
  driver.process

  event = collector.peek

  while !event.nil?
    puts "EVENT: #{event}" if options[:debug]

    case event.type
    when Qpid::Proton::Event::CONNECTION_INIT
      conn = event.connection
      if conn.state & Qpid::Proton::Endpoint::REMOTE_UNINIT
        conn.transport.sasl.done(Qpid::Proton::SASL::OK)
      end

    when Qpid::Proton::Event::CONNECTION_BOUND
      conn = event.connection
      if conn.state & Qpid::Proton::Endpoint::LOCAL_UNINIT
        conn.open
      end

    when Qpid::Proton::Event::CONNECTION_REMOTE_CLOSE
      conn = event.context
      if !(conn.state & Qpid::Proton::Endpoint::LOCAL_CLOSED)
        conn.close
      end

    when Qpid::Proton::Event::SESSION_REMOTE_OPEN
      session = event.session
      if session.state & Qpid::Proton::Endpoint::LOCAL_UNINIT
        session.incoming_capacity = 1000000
        session.open
      end

    when Qpid::Proton::Event::SESSION_REMOTE_CLOSE
      session = event.session
      if !(session.state & Qpid::Proton::Endpoint::LOCAL_CLOSED)
        session.close
      end

    when Qpid::Proton::Event::LINK_REMOTE_OPEN
      link = event.link
      if link.state & Qpid::Proton::Endpoint::LOCAL_UNINIT
        link.open
        link.flow 400
      end

    when Qpid::Proton::Event::LINK_REMOTE_CLOSE
      link = event.context
      if !(link.state & Qpid::Proton::Endpoint::LOCAL_CLOSED)
        link.close
      end

    when Qpid::Proton::Event::DELIVERY
      link = event.link
      delivery = event.delivery
      if delivery.readable? && !delivery.partial?
        # decode the message and display it
        msg = Qpid::Proton::Util::Engine.receive_message(delivery)
        message_count += 1
        puts "Received:"
        puts " Count=#{message_count}" if options[:verbose]
        puts " From=#{msg.id}" if msg.id
        puts " Reply to=#{msg.reply_to}" if msg.reply_to
        puts " Subject=#{msg.subject}" if msg.subject
        puts " Body=#{msg.body}" if msg.body
        puts ""
        delivery.settle
        credit = link.credit
        link.flow(200) if credit <= 200
      end

    when Qpid::Proton::Event::TRANSPORT
      driver.process

    end

    collector.pop
    event = collector.peek
  end
end
