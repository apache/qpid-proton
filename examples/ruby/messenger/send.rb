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

options = {}
messages = []

OptionParser.new do |opts|
  opts.banner = "Usage: send.rb [options] <msg1> ... <msgn>"
  opts.on("-a", "--address [addr]", "The receiver's address (def. 0.0.0.0)") do |f|
    options[:address] = f
  end

  opts.parse!

  messages = ARGV
end

options[:address] = "0.0.0.0" unless options[:address]
messages << "Hello world!" if messages.empty?

messenger = Qpid::Proton::Messenger::Messenger.new
messenger.start
msg = Qpid::Proton::Message.new

messages.each do |message|
  msg.address = options[:address]
  msg.subject = "How are you?"
  msg["sent"] = Time.new
  msg["hostname"] = ENV["HOSTNAME"]
  msg.instructions["fold"] = "yes"
  msg.instructions["spindle"] = "no"
  msg.instructions["mutilate"] = "no"
  msg.annotations["version"] = 1.0
  msg.annotations["pill"] = :RED
  msg.body = message

  begin
    messenger.put(msg)
  rescue Qpid::Proton::ProtonError => error
    puts "ERROR: #{error.message}"
    exit
  end
end

begin
  messenger.send
rescue Qpid::Proton::ProtonError => error
  puts "ERROR: #{error.message}"
  puts error.backtrace.join("\n")
  exit
end

puts "SENT: " + messages.join(",")

messenger.stop
