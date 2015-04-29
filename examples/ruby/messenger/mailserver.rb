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
#

require 'qpid_proton'
require 'optparse'

FAILED         = 0
CONNECTION_UP  = 1
AUTHENTICATING = 2

$options  = {
  :verbose => false,
  :address => ["amqp://~0.0.0.0"],
}

OptionParser.new do |opts|
  opts.banner = "Usage: mailserver [options] <addr_1> ... <addr_n>"

  opts.on("-v", "--verbose", :NONE,
          "Print status messages to stdout") do |f|
    $options[:verbose] = true
  end

  opts.parse!

  if ARGV.length > 0
    $options[:address] = []
    ARGV.each {|address| $options[:address] << address}
  end
end

def log(text)
  STDOUT.puts "#{Time.new}: #{text}" if $options[:verbose]
end

msgr = Qpid::Proton::Messenger::Messenger.new
msgr.start

$options[:address].each {|addr| msgr.subscribe(addr)}

def dispatch(request, response)
  response.subject = "Re: #{request.subject}" if !request.subject.empty?
  response.properties = request.properties
  puts "Dispatched #{request.subject} #{request.properties}"
end

msg = Qpid::Proton::Message.new
reply = Qpid::Proton::Message.new

loop do
  msgr.receive(10) if msgr.incoming < 10

  if msgr.incoming > 0
    msgr.get(msg)
    if !msg.reply_to.nil? && !msg.reply_to.empty?
      puts msg.reply_to
      reply.address = msg.reply_to
      reply.correlation_id = msg.correlation_id
      reply.body = msg.body
    end
    dispatch(msg, reply)
    msgr.put(reply)
    msgr.send
  end
end

msgr.stop
