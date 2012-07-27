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

options = {}
messages = []

OptionParser.new do |opts|
  opts.banner = "Usage: send.rb [options] <msg1> ... <msgn>"
  opts.on("-a", "--address [addr]", "The receiver's address (def. //0.0.0.0)") do |f|
    options[:address] = f
  end

  opts.parse!

  messages = ARGV
end

options[:address] = "//0.0.0.0" unless options[:address]
messages << "Hello world!" if messages.empty?

mng = Cproton::pn_messenger nil

Cproton::pn_messenger_start mng

msg = Cproton::pn_message

messages.each do |message|
  Cproton::pn_message_set_address msg, options[:address]
#  Cproton::pn_message_set_subject msg, "Message sent on #{Time.new}"
  Cproton::pn_message_load msg, message

  if Cproton::pn_messenger_put(mng, msg).nonzero?
    puts "ERROR: #{Cproton::pn_messenger_error mng}"
    exit
  end
end

if Cproton::pn_messenger_send(mng).nonzero?
  puts "ERROR: #{Cproton::pn_messenger_error mng}"
  exit
else
  puts "SENT: " + messages.join(",")
end

Cproton::pn_messenger_stop mng
Cproton::pn_messenger_free mng
