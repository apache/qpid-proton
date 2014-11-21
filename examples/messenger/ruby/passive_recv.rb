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

msgr = Qpid::Proton::Messenger.receive_and_call(nil, :addresses => addresses) do |message|
  puts "Address: #{message.address}"
  subject = message.subject || "(no subject)"
  puts "Subject: #{subject}"
  puts "Body: #{message.body}"
  puts "Properties: #{message.properties}"
  puts "Instructions: #{message.instructions}"
  puts "Annotations: #{message.annotations}"

  if message.reply_to
    puts "=== Sending a reply to #{message.reply_to}"
    reply = Qpid::Proton::Message.new
    reply.address = message.reply_to
    reply.subject = "RE: #{message.subject}"
    reply.content = "Thanks for the message!"

    messenger.put(reply)
    messenger.send
  end

end

Thread.list[1].join
