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

messenger = Qpid::Proton::Messenger::Messenger.new

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

loop do
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
  end
end

messenger.stop

