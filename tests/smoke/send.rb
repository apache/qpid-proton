#!/usr/bin/env ruby

require 'qpid_proton.rb'

messenger = Qpid::Proton::Messenger.new()
messenger.outgoing_window = 10
message = Qpid::Proton::Message.new()

address = ARGV[0]
if not address then
  address = "0.0.0.0"
end

message.address = address
message.properties = {"binding" => "ruby",
  "version" => "#{RUBY_VERSION} #{RUBY_PLATFORM}"}
message.body = "Hello World!"

messenger.start()
tracker = messenger.put(message)
print "Put: #{message}\n"
messenger.send()
print "Status: ", messenger.status(tracker), "\n"
messenger.stop()
