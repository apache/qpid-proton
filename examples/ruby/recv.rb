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

require 'cproton'
require 'optparse'

addresses = []

OptionParser.new do |opts|
  opts.banner = "Usage: recv.rb <addr1> ... <addrn>"
  opts.parse!

  addresses = ARGV
end

addresses = ["//~0.0.0.0"] if addresses.empty?

mng = Cproton::pn_messenger nil

if Cproton::pn_messenger_start(mng).nonzero?
  puts "ERROR: #{Cproton::pn_messenger_error mng}"
end

addresses.each do |address|
  if Cproton::pn_messenger_subscribe(mng, address).nonzero?
    puts "ERROR: #{Cproton::pn_messenger_error(mng)}"
    exit
  end
end

msg = Cproton::pn_message

loop do
  if Cproton::pn_messenger_recv(mng, 10).nonzero?
    puts "ERROR: #{Cproton::pn_messenger_error mng}"
    exit
  end

  while Cproton::pn_messenger_incoming(mng).nonzero?
    if Cproton::pn_messenger_get(mng, msg).nonzero?
      puts "ERROR: #{Cproton::pn_messenger_error mng}"
      exit
    else
      (cd, body) = Cproton::pn_message_save(msg, 1024)
      puts "Address: #{Cproton::pn_message_get_address msg}"
      subject = Cproton::pn_message_get_subject(msg) || "(no subject)"
      puts "Subject: #{subject}"
      puts "Content: #{body}"
    end
  end
end

Cproton::pn_messenger_stop mng
Cproton::pn_messenger_free mng
