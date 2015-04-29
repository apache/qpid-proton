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


$options  = {
  :verbose => false,
  :hostname => "0.0.0.0",
  :subject => "",
  :replyto => "~/replies"
}


OptionParser.new do |opts|
  opts.banner = "Usage: client [options] <addr> <subject>"

  opts.on("-r", "--reply-to", String, :REQUIRED,
          "Reply address") do |replyto|
    $options[:replyto] = replyto
  end

  opts.on("-v", "--verbose", :NONE,
          "Enable verbose output") do
    $options[:verbose] = true
  end

  opts.on("-h", "--help", :NONE,
          "Show this help message") do
    puts opts
    exit
  end

  begin
    ARGV << "-h" if ARGV.empty?
    opts.parse!(ARGV)
  rescue OptionParser::ParseError => error
    STDERR.puts error.message, "\n", opts
    exit 1
  end

  ($options[:address], $options[:subject]) = ARGV

  abort "No address specified" if $options[:hostname].nil?
  abort "No subject specified" if $options[:subject].nil?

end

def log(text)
  printf "#{Time.new}: #{text}\n" if $options[:verbose]
end

msgr = Qpid::Proton::Messenger::Messenger.new
msgr.start

msg = Qpid::Proton::Message.new
msg.address = $options[:address]
msg.subject = $options[:subject]
msg.reply_to = $options[:replyto]

msgr.put(msg)
msgr.send

if $options[:replyto].start_with? "~/"
  msgr.receive(1)
  begin
    msgr.get(msg)
    puts "#{msg.address}, #{msg.subject}"
  rescue error
    puts error
  end
end

msgr.stop
