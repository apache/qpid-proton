#!/usr/bin/env ruby
#
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

require 'qpid_proton'
require 'optparse'

addresses = []

OptionParser.new do |opts|
  opts.banner = "Usage: recv.rb <addr1> ... <addrn>"
  opts.parse!

  addresses = ARGV
end

addresses = ["~0.0.0.0"] if addresses.empty?

messenger = Qpid::Proton::Messenger.new
messenger.passive = true

begin
  messenger.start
rescue ProtonError => error
  puts "ERROR: #{error.message}"
  puts error.backtrace.join("\n")
  exit
end

addresses.each do |address|
  begin
    messenger.subscribe(address)
  rescue Qpid::Proton::ProtonError => error
    puts "ERROR: #{error.message}"
    exit
  end
end

msg = Qpid::Proton::Message.new

read_array = []
write_array = []
selectables = {}

loop do

  # wait for incoming messages
  sel = messenger.selectable
  while !sel.nil?
    if sel.terminal?
      selectables.delete(sel.fileno)
      read_array.delete(sel)
      write_array.delete(sel)
      sel.free
    else
      sel.capacity
      sel.pending
      if !sel.registered?
        read_array << sel
        write_array << sel
        selectables[sel.fileno] = sel
        sel.registered = true
      end
    end
    sel = messenger.selectable
  end

  unless selectables.empty?
    rarray = []; read_array.each {|fd| rarray << fd.to_io }
    warray = []; write_array.each {|fd| warray << fd.to_io }

    if messenger.deadline > 0.0
      result = IO.select(rarray, warray, nil, messenger.deadline)
    else
      result = IO.select(rarray, warray)
    end

    unless result.nil? && result.empty?
      result.flatten.each do |io|
        sel = selectables[io.fileno]

        sel.writable if sel.pending > 0
        sel.readable if sel.capacity > 0
      end
    end

    begin
      messenger.receive(10)
    rescue Qpid::Proton::ProtonError => error
      puts "ERROR: #{error.message}"
      exit
    end

    while messenger.incoming.nonzero?
      begin
        messenger.get(msg)
      rescue Qpid::Proton::Error => error
        puts "ERROR: #{error.message}"
        exit
      end

      puts "Address: #{msg.address}"
      subject = msg.subject || "(no subject)"
      puts "Subject: #{subject}"
      puts "Body: #{msg.body}"
      puts "Properties: #{msg.properties}"
      puts "Instructions: #{msg.instructions}"
      puts "Annotations: #{msg.annotations}"

      if msg.reply_to
        puts "=== Sending a reply to #{msg.reply_to}"
        reply = Qpid::Proton::Message.new
        reply.address = msg.reply_to
        reply.subject = "RE: #{msg.subject}"
        reply.content = "Thanks for the message!"

        messenger.put(reply)
        messenger.send
      end
    end
  end
end

messenger.stop

