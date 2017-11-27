#!/usr/bin/env ruby

require 'qpid_proton.rb'

messenger = Qpid::Proton::Messenger::Messenger.new()
messenger.outgoing_window = 10
message = Qpid::Proton::Message.new()

address = ARGV[0]
if not address then
  address = "0.0.0.0"
end

message.address = address
message.body = "Hello World!"

messenger.start()
tracker = messenger.put(message)
messenger.send()
print "Status: ", messenger.status(tracker), "\n"
messenger.stop()
