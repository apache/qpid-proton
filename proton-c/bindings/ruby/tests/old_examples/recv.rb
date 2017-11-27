#!/usr/bin/env ruby

require 'qpid_proton.rb'

messenger = Qpid::Proton::Messenger::Messenger.new()
messenger.incoming_window = 1
message = Qpid::Proton::Message.new()

address = ARGV[0]
if not address then
  address = "~0.0.0.0"
end
messenger.subscribe(address)

messenger.start()

puts "Listening"; STDOUT.flush
messenger.receive()
messenger.get(message)
puts "Got: #{message.body}"
messenger.accept()

messenger.stop()
