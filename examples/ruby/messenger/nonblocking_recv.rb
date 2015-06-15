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

Thread.new do
  print "This is a side thread:\n"
  loop do
    print "The time is now #{Time.new.strftime('%I:%M:%S')}.\n"
    sleep 1
  end
end

addresses = []

OptionParser.new do |opts|
  opts.banner = "Usage: recv.rb <addr1> ... <addrn>"
  opts.parse!

  addresses = ARGV
end

addresses = ["~0.0.0.0"] if addresses.empty?

messenger = Qpid::Proton::Messenger::Messenger.new
messenger.passive = true

begin
  messenger.start
rescue ProtonError => error
  print "ERROR: #{error.message}\n"
  print error.backtrace.join("\n")
  exit
end

addresses.each do |address|
  begin
    messenger.subscribe(address)
  rescue Qpid::Proton::ProtonError => error
    print "ERROR: #{error.message}\n"
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

        sel.writable
        sel.readable
      end
    end

    begin
      messenger.receive(10)
    rescue Qpid::Proton::ProtonError => error
      print "ERROR: #{error.message}\n"
      exit
    end

    while messenger.incoming.nonzero?
      begin
        messenger.get(msg)
      rescue Qpid::Proton::Error => error
        print "ERROR: #{error.message}\n"
        exit
      end

      print "Address: #{msg.address}\n"
      subject = msg.subject || "(no subject)"
      print "Subject: #{subject}\n"
      print "Body: #{msg.body}\n"
      print "Properties: #{msg.properties}\n"
      print "Instructions: #{msg.instructions}\n"
      print "Annotations: #{msg.annotations}\n"

      if msg.reply_to
        print "=== Sending a reply to #{msg.reply_to}\n"
        reply = Qpid::Proton::Message.new
        reply.address = msg.reply_to
        reply.subject = "RE: #{msg.subject}"
        reply.body = "Thanks for the message!"

        messenger.put(reply)
        messenger.send
      end
    end
  end
end

messenger.stop
